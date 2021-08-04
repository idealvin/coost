#include "co/so/http.h"
#include "co/so/tcp.h"
#include "co/co.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/fastring.h"
#include "co/fastream.h"
#include "co/str.h"
#include "co/time.h"
#include "co/fs.h"
#include "co/path.h"
#include "co/lru_map.h"
#include <memory>
#include <unordered_map>

#ifdef HAS_LIBCURL
#include <curl/curl.h>
#endif

DEF_uint32(http_max_header_size, 4096, "#2 max size of http header");
DEF_uint32(http_max_body_size, 8 << 20, "#2 max size of http body, default: 8M");
DEF_uint32(http_timeout, 3000, "#2 send or recv timeout in ms for http client");
DEF_uint32(http_conn_timeout, 3000, "#2 connect timeout in ms for http client");
DEF_uint32(http_recv_timeout, 3000, "#2 recv timeout in ms for http server");
DEF_uint32(http_send_timeout, 3000, "#2 send timeout in ms for http server");
DEF_uint32(http_conn_idle_sec, 180, "#2 http server may close the connection if no data was recieved for n seconds");
DEF_uint32(http_max_idle_conn, 128, "#2 max idle connections for http server");
DEF_bool(http_log, true, "#2 enable http server log if true");

#define HTTPLOG LOG_IF(FLG_http_log)

namespace http {

/**
 * ===========================================================================
 * HTTP client 
 *   - libcurl & zlib required. 
 *   - openssl required for https. 
 * ===========================================================================
 */

#ifdef HAS_LIBCURL
struct curl_ctx {
    curl_ctx()
        : l(NULL), upload(NULL), upsize(0), s(-1), cs(-1), action(0), ms(0) {
        multi = curl_multi_init();
        easy = curl_easy_init();
        memset(err, 0, sizeof(err));
    }

    ~curl_ctx() {
        curl_slist_free_all(l);     l = NULL;
        curl_easy_cleanup(easy);    easy = NULL;
        curl_multi_cleanup(multi);  multi = NULL;
    }

    fastring serv_url;
    fastream stream;
    fastream header;
    fastream mutable_header;
    fastream body;
    std::vector<size_t> header_index;
    char err[CURL_ERROR_SIZE];

    CURLM* multi;
    CURL* easy;
    struct curl_slist* l;
    const char* upload; // for PUT, data to upload
    size_t upsize;      // for PUT, size of the data to upload
    curl_socket_t s;
    curl_socket_t cs;
    int action;
    int ms;
};

static int multi_socket_cb(CURL* easy, curl_socket_t s, int action, void* userp, void* socketp);
static int multi_timer_cb(CURLM* m, long ms, void* userp);
static size_t easy_write_cb(void* data, size_t size, size_t count, void* userp);
static size_t easy_read_cb(char* p, size_t size, size_t nmemb, void* userp);
static size_t easy_header_cb(char* p, size_t size, size_t nmemb, void* userp);
static curl_socket_t easy_opensocket_cb(void* userp, curlsocktype purpose, struct curl_sockaddr* addr);
static int easy_closesocket_cb(void* userp, curl_socket_t fd);
static int easy_sockopt_cb(void* userp, curl_socket_t fd, curlsocktype purpose);

void init_multi_opts(CURLM* m, curl_ctx* ctx) {
    curl_multi_setopt(m, CURLMOPT_SOCKETFUNCTION, multi_socket_cb);
    curl_multi_setopt(m, CURLMOPT_SOCKETDATA, (void*)ctx); // userdata
    curl_multi_setopt(m, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
    curl_multi_setopt(m, CURLMOPT_TIMERDATA, (void*)ctx);
}

void init_easy_opts(CURL* e, curl_ctx* ctx) {
    curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(e, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(e, CURLOPT_SSL_VERIFYHOST, 0);

    curl_easy_setopt(e, CURLOPT_HEADERFUNCTION, easy_header_cb);
    curl_easy_setopt(e, CURLOPT_HEADERDATA, (void*)ctx);
    curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, easy_write_cb);
    curl_easy_setopt(e, CURLOPT_WRITEDATA, (void*)ctx);

    curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, FLG_http_conn_timeout);
    curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, FLG_http_timeout);

    curl_easy_setopt(e, CURLOPT_OPENSOCKETFUNCTION, easy_opensocket_cb);
    curl_easy_setopt(e, CURLOPT_OPENSOCKETDATA, (void*)ctx);
    curl_easy_setopt(e, CURLOPT_CLOSESOCKETFUNCTION, easy_closesocket_cb);
    curl_easy_setopt(e, CURLOPT_CLOSESOCKETDATA, (void*)ctx);
    curl_easy_setopt(e, CURLOPT_SOCKOPTFUNCTION, easy_sockopt_cb);

    curl_easy_setopt(e, CURLOPT_ERRORBUFFER, ctx->err);
}

struct curl_global {
    curl_global() {
        const bool x = curl_global_init(CURL_GLOBAL_ALL) == 0;
        CHECK(x) << "curl global init failed..";
    }

    ~curl_global() {
        curl_global_cleanup();
    }
};

Client::Client(const char* serv_url) {
    static curl_global g;
    _ctx = new curl_ctx();

    fastring s(serv_url);
    s.strip('/', 'r');        // remove '/' at the right side
    if (!s.starts_with("https://") && !s.starts_with("http://")) {
        s = "http://" + s;    // use http by default if protocol was not specified.
    }
    _ctx->serv_url = std::move(s);

    init_multi_opts(_ctx->multi, _ctx);
    init_easy_opts(_ctx->easy, _ctx);
}

Client::~Client() {
    if (_ctx) { delete _ctx; _ctx = NULL; }
}

void Client::close() {
    if (_ctx) { delete _ctx; _ctx = NULL; }
}

inline void Client::append_header(const char* s) {
    struct curl_slist* l = curl_slist_append(_ctx->l, s);
    if (l) _ctx->l = l;
    if (!l) ELOG << "libcurl add header failed: " << s;
}

void Client::add_header(const char* key, const char* val) {
    auto& s = _ctx->stream;
    s.clear();
    s.append(key);
    const size_t n = strlen(val);
    if (n > 0) {
        s.append(": ").append(val, n);
    } else {
        s.append(";");
    }
    this->append_header(s.c_str());
}

void Client::add_header(const char* key, int val) {
    auto& s = _ctx->stream;
    s.clear();
    s << key << ": " << val;
    this->append_header(s.c_str());
}

void Client::remove_header(const char* key) {
    auto& s = _ctx->stream;
    s.clear();
    s.append(key).append(':');
    this->append_header(s.c_str());
}

inline void Client::set_headers() {
    if (_ctx->l) {
        curl_easy_setopt(_ctx->easy, CURLOPT_HTTPHEADER, _ctx->l);
        curl_slist_free_all(_ctx->l);
        _ctx->l = NULL;
    }
}

inline const char* Client::make_url(const char* url) {
    auto& s = _ctx->stream;
    s.clear();
    s.append(_ctx->serv_url).append(url);
    return s.c_str();
}

void Client::set_url(const char* url) {
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
}

void Client::get(const char* url) {
    curl_easy_setopt(_ctx->easy, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    this->perform();
}

void Client::post(const char* url, const char* data, size_t size) {
    curl_easy_setopt(_ctx->easy, CURLOPT_POST, 1L);
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    curl_easy_setopt(_ctx->easy, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(_ctx->easy, CURLOPT_POSTFIELDSIZE, (long)size);
    this->perform();
}

void Client::head(const char* url) {
    curl_easy_setopt(_ctx->easy, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    this->perform();
}

void Client::del(const char* url, const char* data, size_t size) {
    curl_easy_setopt(_ctx->easy, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    curl_easy_setopt(_ctx->easy, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(_ctx->easy, CURLOPT_POSTFIELDSIZE, (long)size);
    this->perform();
    curl_easy_setopt(_ctx->easy, CURLOPT_CUSTOMREQUEST, NULL);
}

void Client::put(const char* url, const char* data, size_t size) {
    _ctx->upload = data;
    _ctx->upsize = size;
    curl_easy_setopt(_ctx->easy, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    curl_easy_setopt(_ctx->easy, CURLOPT_READFUNCTION, easy_read_cb);
    curl_easy_setopt(_ctx->easy, CURLOPT_READDATA, (void*)_ctx);
    this->perform();
}

void* Client::easy_handle() const {
    return _ctx->easy;
}

void Client::perform() {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    this->set_headers();
    _ctx->header.clear();
    _ctx->body.clear();
    _ctx->header_index.clear();
    _ctx->err[0] = '\0';
    curl_multi_add_handle(_ctx->multi, _ctx->easy);
    while (_ctx->action != 0) do_io();
    _ctx->mutable_header.clear();
}

int Client::response_code() const {
    long code = 0;
    const CURLcode r = curl_easy_getinfo(_ctx->easy, CURLINFO_RESPONSE_CODE, &code);
    return r == CURLE_OK ? (int)code : 0;
}

const char* Client::strerror() const {
    if (_ctx->err[0]) return _ctx->err;
    if (co::error() != 0) return co::strerror();
    return "ok";
}

const char* Client::header(const char* key) {
    static const char* e = "";
    auto& index = _ctx->header_index;
    if (index.empty()) return e;

    fastream& h = _ctx->header;
    fastream& m = _ctx->mutable_header;
    if (m.empty() && !h.empty()) m.append(h.c_str(), h.size() + 1);

    fastring& s = *(fastring*)(&_ctx->stream);
    fastring u(std::move(fastring(key).toupper()));
    const char* header_begin = m.c_str();
    const char* b;
    const char* p;
    for (size_t i = 0; i < index.size(); ++i) {
        b = header_begin + index[i];
        p = strchr(b, ':');
        if (p) {
            s.clear();
            s.append(b, p - b).strip(' ').toupper();
            if (s != u) continue;

            b = p;
            while (*++b == ' ');
            p = strchr(b, '\r');
            if (p) *(char*)p = '\0';
            return b;
        }
    }
    return e;
}

const char* Client::header() const {
    return _ctx->header.c_str();
}

const char* Client::body() const {
    return _ctx->body.data();
}

size_t Client::body_size() const {
    return _ctx->body.size();
}

void Client::do_io() {
    bool r;
    int n, curl_ev;

    if (_ctx->action == CURL_POLL_IN) {
        curl_ev = CURL_CSELECT_IN;
        co::IoEvent ev(_ctx->s, co::ev_read);
        r = ev.wait(_ctx->ms);
    } else if (_ctx->action == CURL_POLL_OUT) {
        curl_ev = CURL_CSELECT_OUT;
        co::IoEvent ev(_ctx->s, co::ev_write);
        r = ev.wait(_ctx->ms);
    } else {
        CHECK_EQ(_ctx->action, CURL_POLL_IN | CURL_POLL_OUT);
        curl_ev = CURL_CSELECT_IN | CURL_CSELECT_OUT;
        do {
            {
                co::IoEvent ev(_ctx->s, co::ev_write);
                r = ev.wait(_ctx->ms);
                if (!r) break;
            }
            {
                co::IoEvent ev(_ctx->s, co::ev_read);
                r = ev.wait(_ctx->ms);
            }
        } while (0);
    }

    if (r) {
        curl_multi_socket_action(_ctx->multi, _ctx->s, curl_ev, &n);
    } else {
        if (co::timeout()) {
            curl_multi_socket_action(_ctx->multi, CURL_SOCKET_TIMEOUT, 0, &n);
        } else {
            ELOG << "io wait error: " << co::strerror();
        }
    }

    CURLMsg* msg;
    while ((msg = curl_multi_info_read(_ctx->multi, &n))) {
        switch (msg->msg) {
        case CURLMSG_DONE:
            _ctx->action = 0;
            curl_multi_remove_handle(_ctx->multi, msg->easy_handle);
            break;
        default:
            CHECK(false) << "CURLMSG default";
        }
    }
}

int multi_socket_cb(CURL* easy, curl_socket_t s, int action, void* userp, void* socketp) {
    CHECK(userp != NULL);
    curl_ctx* ctx = (curl_ctx*)userp;
    switch (action) {
      case CURL_POLL_IN:
      case CURL_POLL_OUT:
      case CURL_POLL_IN | CURL_POLL_OUT:
        ctx->s = s;
        ctx->action = action;
        break;
      case CURL_POLL_REMOVE:
        // delete io event if the socket was not created by the opensocket callback.
        // Now we have hooked close (closesocket) globally, MAYBE no need to do this..
        if (s != ctx->cs) co::del_io_event(s);
        ctx->s = 0;
        ctx->action = 0;
        break;
      default:
        CHECK(false) << "invalid curl action: " << action;
    }
    return 0;
}

int multi_timer_cb(CURLM* m, long ms, void* userp) {
    int n;
    curl_ctx* ctx = (curl_ctx*)userp;
    if (ms == 0) {
        CHECK_EQ(m, ctx->multi);
        curl_multi_socket_action(m, CURL_SOCKET_TIMEOUT, 0, &n);
    } else {
        if (ms > 0) ctx->ms = ms;
    }
    return 0;
}

size_t easy_write_cb(void* data, size_t size, size_t count, void* userp) {
    curl_ctx* ctx = (curl_ctx*)userp;
    const size_t n = size * count;
    ctx->body.append(data, n);
    return n;
}

size_t easy_header_cb(char* p, size_t size, size_t nmemb, void* userp) {
    curl_ctx* ctx = (curl_ctx*)userp;
    const size_t n = size * nmemb;
    fastream& h = ctx->header;

    long code = 0;
    const CURLcode r = curl_easy_getinfo(ctx->easy, CURLINFO_RESPONSE_CODE, &code);
    if (code == 100) return n;

    if (!h.empty()) {
        if (n > 2) ctx->header_index.push_back(h.size());
        h.append(p, n);
    } else {
        h.append(p, n); // start line
    }
    return n;
}

size_t easy_read_cb(char* p, size_t size, size_t nmemb, void* userp) {
    curl_ctx* ctx = (curl_ctx*)userp;
    if (ctx->upsize == 0) return 0;
    const size_t N = size * nmemb;
    const size_t n = (ctx->upsize <= N ? ctx->upsize : N);
    memcpy(p, ctx->upload, n);
    ctx->upload += n;
    ctx->upsize -= n;
    return n;
}

curl_socket_t easy_opensocket_cb(void* userp, curlsocktype purpose, struct curl_sockaddr* addr) {
    curl_ctx* ctx = (curl_ctx*)userp;
    curl_socket_t fd = co::socket(addr->family, addr->socktype, addr->protocol);
    if (fd == (sock_t)-1) {
        ELOG << "create socket failed: " << co::strerror();
        return CURL_SOCKET_BAD;
    }

    const int r = co::connect(fd, &addr->addr, addr->addrlen, FLG_http_conn_timeout);
    if (r == 0) return (ctx->cs = fd);

    ELOG << "connect to " << co::to_string(&addr->addr, addr->addrlen) << " failed: " << co::strerror();
    memcpy(ctx->err, "connect timeout", 16);
    co::close(fd);
    return CURL_SOCKET_BAD;
}

int easy_closesocket_cb(void* userp, curl_socket_t fd) {
    if (fd != (curl_socket_t)-1) return co::close(fd);
    return 0;
}

int easy_sockopt_cb(void* userp, curl_socket_t fd, curlsocktype purpose) {
    return CURL_SOCKOPT_ALREADY_CONNECTED;
}

#else
Client::Client(const char*) {
    CHECK(false)
        << "To use http::Client, please build libco with libcurl as follow: \n"
        << "xmake f --with_libcurl=true\n"
        << "xmake -v";
}

Client::~Client() {}
void Client::add_header(const char*, const char*) {}
void Client::add_header(const char*, int) {}
void Client::remove_header(const char*) {}
void Client::get(const char*) {}
void Client::head(const char*) {}
void Client::post(const char*, const char*, size_t) {}
void Client::put(const char*, const char*, size_t) {}
void Client::del(const char*, const char*, size_t) {}
void Client::set_url(const char*) {}
void* Client::easy_handle() const { return 0; }
void Client::perform() {}
int Client::response_code() const { return 0; }
const char* Client::strerror() const { return ""; }
const char* Client::header(const char* key) { return ""; }
const char* Client::header() const { return ""; }
const char* Client::body() const { return ""; }
size_t Client::body_size() const { return 0; }
void Client::close() {}

#endif // http::Client


/**
 * ===========================================================================
 * HTTP server 
 *   - openssl required for https. 
 * ===========================================================================
 */

inline const char* version_str(int v) {
    static const char* s[] = { "HTTP/1.0", "HTTP/1.1", "HTTP/2.0" };
    return s[v];
}

inline const char* method_str(int m) {
    static const char* s[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS" };
    return s[m];
}

const char** create_status_table() {
    static const char* s[512];
    for (int i = 0; i < 512; ++i) s[i] = "";
    s[100] = "Continue";
    s[101] = "Switching Protocols";
    s[200] = "OK";
    s[201] = "Created";
    s[202] = "Accepted";
    s[203] = "Non-authoritative Information";
    s[204] = "No Content";
    s[205] = "Reset Content";
    s[206] = "Partial Content";
    s[300] = "Multiple Choices";
    s[301] = "Moved Permanently";
    s[302] = "Found";
    s[303] = "See Other";
    s[304] = "Not Modified";
    s[305] = "Use Proxy";
    s[307] = "Temporary Redirect";
    s[400] = "Bad Request";
    s[401] = "Unauthorized";
    s[402] = "Payment Required";
    s[403] = "Forbidden";
    s[404] = "Not Found";
    s[405] = "Method Not Allowed";
    s[406] = "Not Acceptable";
    s[407] = "Proxy Authentication Required";
    s[408] = "Request Timeout";
    s[409] = "Conflict";
    s[410] = "Gone";
    s[411] = "Length Required";
    s[412] = "Precondition Failed";
    s[413] = "Payload Too Large";
    s[414] = "Request-URI Too Long";
    s[415] = "Unsupported Media Type";
    s[416] = "Requested Range Not Satisfiable";
    s[417] = "Expectation Failed";
    s[500] = "Internal Server Error";
    s[501] = "Not Implemented";
    s[502] = "Bad Gateway";
    s[503] = "Service Unavailable";
    s[504] = "Gateway Timeout";
    s[505] = "HTTP Version Not Supported";
    return s;
}

inline const char* status_str(int n) {
    static const char** s = create_status_table();
    return (100 <= n && n <= 511) ? s[n] : s[500];
}

class ServerImpl {
  public:
    ServerImpl();
    ~ServerImpl() = default;

    void on_req(std::function<void(const Req&, Res&)>&& f) {
        _on_req = std::move(f);
    }

    void start(const char* ip, int port, const char* key, const char* ca);

  private:
    void on_connection(tcp::Connection* conn);

  private:
    co::Pool _buffer; // buffer for recieving http data
    uint32 _conn_num;
    std::unique_ptr<tcp::Server> _serv;
    std::function<void(const Req&, Res&)> _on_req;
};

Server::Server() {
    _p = new ServerImpl();
}

Server::~Server() {
    delete (ServerImpl*)_p;
}

void Server::on_req(std::function<void(const Req&, Res&)>&& f) {
    ((ServerImpl*)_p)->on_req(std::move(f));
}

void Server::start(const char* ip, int port) {
    ((ServerImpl*)_p)->start(ip, port, NULL, NULL);
}

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    ((ServerImpl*)_p)->start(ip, port, key, ca);
}

ServerImpl::ServerImpl()
    : _buffer(
          []() { return (void*) new fastring(4096); },
          [](void* p) { delete (fastring*)p; }
      ), _conn_num(0) {
}

void ServerImpl::start(const char* ip, int port, const char* key, const char* ca) {
    CHECK(_on_req != NULL) << "req callback must be set..";
    _serv.reset(new tcp::Server());
    _serv->on_connection(&ServerImpl::on_connection, this);
    _serv->start(ip, port, key, ca);
}

static inline int hex2int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_headers(const char* x, const char* beg, std::vector<size_t>& headers);

void send_error_message(int err, Res& res, tcp::Connection* conn) {
    res.set_status(err);
    fastring s = res.str();
    conn->send(s.data(), (int)s.size(), FLG_http_send_timeout);
    HTTPLOG << "http send res: " << s;
    res.clear();
}

void ServerImpl::on_connection(tcp::Connection* conn) {
    std::unique_ptr<tcp::Connection> x(conn);
    char c;
    int r = 0;
    size_t pos = 0, total_len = 0;
    fastring* buf = 0;
    Req req; Res res;

    r = atomic_inc(&_conn_num);
    DLOG << "http conn num: " << r;

    while (true) {
        {
          recv_beg:
            if (!buf) {
                // try recieving a single byte
                r = conn->recv(&c, 1, FLG_http_conn_idle_sec * 1000);
                if (r == 0) goto recv_zero_err;
                if (r < 0) {
                    if (!co::timeout()) goto recv_err;
                    if (_conn_num > FLG_http_max_idle_conn) goto idle_err;
                    goto recv_beg;
                }
                buf = (fastring*) _buffer.pop();
                buf->clear();
                buf->append(c);
            }

            // recv until the entire http header was done. 
            while ((pos = buf->find("\r\n\r\n")) == buf->npos) {
                if (buf->size() > FLG_http_max_header_size) goto header_too_long_err;
                buf->reserve(buf->size() + 1024);
                r = conn->recv(
                    (void*)(buf->data() + buf->size()), 
                    (int)(buf->capacity() - buf->size()), FLG_http_recv_timeout
                );
                if (r == 0) goto recv_zero_err;
                if (r < 0) goto recv_err;
                buf->resize(buf->size() + r);
            }

            // parse http header
            req._body = pos + 4;
            (*buf)[pos + 2] = '\0'; // make header null-terminated
            HTTPLOG << "http recv req: " << buf->data();
            r = req.parse(buf);
            if (r != 0) { /* parse error */
                ELOG << "http parse error: " << r;
                send_error_message(r, res, conn);
                goto err_end;
            }

            // try to recv the remain part of http body
            if (req.body_size() > 0 || strcmp(req.header("Transfer-Encoding"), "chunked") != 0) {
                total_len = pos + 4 + req.body_size();
                if (req.body_size() > 0) {
                    if (buf->size() < total_len) {
                        buf->reserve(total_len);
                        r = conn->recvn(
                            (void*)(buf->data() + buf->size()), 
                            (int)(total_len - buf->size()), FLG_http_recv_timeout
                        );
                        if (r == 0) goto recv_zero_err;
                        if (r < 0) goto recv_err;
                        buf->resize(total_len);
                    }
                }

            } else { /* stupid chunked data */
                // he grandmother's. The chunked Transfer-Encoding makes the stupid HTTP stupid again.
                bool expect_100_continue = strcmp(req.header("Expect"), "100-continue") == 0;
                size_t x, o, i, n = 0;
                fastring& s = req._s;
                s.clear();
                if (buf->size() > pos + 4) {
                    s.append(buf->data() + pos + 4, buf->size() - pos - 4);
                    buf->resize(pos + 4);
                }

                while (true) {
                    while ((x = s.find("\r\n")) == s.npos) {
                        if (expect_100_continue) { /* send 100 continue */
                            res.set_version(req.version());
                            send_error_message(100, res, conn);
                        }
                        s.reserve(s.size() + 32);
                        r = conn->recv((void*)(s.data() + s.size()), 32, FLG_http_recv_timeout);
                        if (r == 0) goto recv_zero_err;
                        if (r < 0) goto recv_err;
                        s.resize(s.size() + r);
                    }

                    if (x == 0) { s.lshift(2); continue; }

                    // chunked data:  1a[;xxx]\r\n data\r\n
                    s[x] = '\0';
                    if ((o = s.find(';')) == s.npos) o = x;
                    for (i = 0, n = 0; i < o; ++i) {
                        if ((r = hex2int(s[i])) < 0) goto chunk_err;
                        n = (n << 4) + r;
                    }

                    if (n > 0) {
                        o = s.size() - x - 2;
                        if (o < n) {
                            buf->append(s.data() + x + 2, o);
                            buf->reserve(buf->size() + n - o + 2);
                            r = conn->recvn((void*)(buf->data() + buf->size()), (int)(n - o + 2), FLG_http_recv_timeout);
                            if (r == 0) goto recv_zero_err;
                            if (r < 0) goto recv_err;
                            buf->resize(buf->size() + r - 2);
                            s.clear();
                        } else {
                            buf->append(s.data() + x + 2, n);
                            s.lshift(s.size() >= x + n + 4 ? x + 4 + n : x + 2 + n);
                        }

                        if (buf->size() - pos - 4 > FLG_http_max_body_size) {
                            send_error_message(413, res, conn);
                            goto err_end;
                        }

                    } else { /* n == 0, end of chunked data */
                        req._body_size = buf->size() - pos - 4;
                        s[x] = '\r'; s.lshift(x);
                        while ((x = s.find("\r\n\r\n")) == s.npos) {
                            s.reserve(s.size() + 32);
                            r = conn->recv((void*)(s.data() + s.size()), 32, FLG_http_recv_timeout);
                            if (r == 0) goto recv_zero_err;
                            if (r < 0) goto recv_err;
                            s.resize(s.size() + r);
                        }

                        s[x + 2] = '\0';
                        if (s.size() > 4) {
                            // there are some tailing headers following the chunked data
                            total_len = buf->size() + x + 4;
                            o = buf->size();
                            buf->append(s.data(), s.size());
                            parse_headers(buf->data() + o + 2, buf->data(), req._headers);
                        } else {
                            total_len = buf->size();
                        }

                        break; // exit the while loop
                    }
                }
            };
        };

        { /* handle the http request */
            bool need_close = false;
            fastring x(req.header("Connection")); 
            if (!x.empty()) res.add_header("Connection", x.c_str());
            res.set_version(req.version());

            if (req.version() == kHTTP10) {
                if (x.empty() || x.lower() != "keep-alive") need_close = true;
            } else {
                if (!x.empty() && x == "close") need_close = true;
            }

            _on_req(req, res);
            fastring s = res.str();
            r = conn->send(s.data(), (int)s.size(), FLG_http_send_timeout);
            if (r <= 0) goto send_err;

            s.resize(s.size() - res.body_size());
            HTTPLOG << "http send res: " << s;
            if (need_close) { conn->close(0); goto cleanup; }
        };

        if (buf->size() == total_len) {
            buf->clear();
            _buffer.push(buf);
            buf = 0;
        } else {
            buf->lshift(total_len);
        }

        req.clear();
        res.clear();
        total_len = 0;
    }

  recv_zero_err:
    LOG << "http client close the connection: " << co::peer(conn->socket()) << ", connfd: " << conn->socket();
    conn->close();
    goto cleanup;
  idle_err:
    LOG << "http close idle connection: " << co::peer(conn->socket()) << ", connfd: " << conn->socket();
    conn->reset();
    goto cleanup;
  header_too_long_err:
    ELOG << "http recv error: header too long";
    goto err_end;
  recv_err:
    ELOG << "http recv error: " << conn->strerror();
    goto err_end;
  send_err:
    ELOG << "http send error: " << conn->strerror();
    goto err_end;
  chunk_err:
    ELOG << "http invalid chunked data..";
    goto err_end;
  err_end:
    conn->reset(1000);
  cleanup:
    atomic_dec(&_conn_num);
    if (buf) { 
        buf->clear();
        buf->capacity() <= 1024 * 1024 ? _buffer.push(buf) : delete buf;
    }
}

const char* Req::header(const char* key) const {
    static const char* e = "";
    Req* req = (Req*)this;
    fastring& s = req->_s;
    fastring x(key);
    x.toupper();

    for (size_t i = 0; i < _headers.size(); i += 2) {
        s.clear();
        s.append(_buf->data() + _headers[i]).toupper();
        if (s == x) return _buf->data() + _headers[i + 1];
    }
    return e;
}

fastring Res::str() const {
    fastring s(_header.size() + _body.size() + 64);
    s << version_str(_version) << ' ' << _status << ' ' << status_str(_status) << "\r\n";
    s << "Content-Length: " << _body.size() << "\r\n";
    s << _header << "\r\n";
    if (_body.size() > 0) s << _body;
    return s;
}

static std::unordered_map<fastring, int>* method_map() {
    static std::unordered_map<fastring, int> m;
    m["GET"]     = kGet;
    m["POST"]    = kPost;
    m["HEAD"]    = kHead;
    m["PUT"]     = kPut;
    m["DELETE"]  = kDelete;
    m["OPTIONS"] = kOptions;
    return &m;
}

static std::unordered_map<fastring, int>* version_map() {
    static std::unordered_map<fastring, int> m;
    m["HTTP/1.0"] = kHTTP10;
    m["HTTP/1.1"] = kHTTP11;
    return &m;
}

int parse_headers(const char* x, const char* beg, std::vector<size_t>& headers) {
    const char* p;
    while (*x) {
        p = strchr(x, '\r');
        if (!p || *(p + 1) != '\n') return 400;

        *(char*)p = '\0';
        headers.push_back(x - beg); // key

        x = strchr(x, ':');
        if (!x) return 400;

        *(char*)x = '\0';
        while (*++x == ' ');
        headers.push_back(x - beg); // value
        x = p + 2;
    }
    return 0;
}

int Req::parse(fastring* buf) {
    _buf = buf;
    const char* s = _buf->data(); 
    const char* x = strchr(s, '\r'); // MUST NOT be NULL

    { /* parse start line */
        const char* p;
        const char* q;
        const char* beg = s;
        const char* end = x;
        if (*(end + 1) != '\n') return 400;

        *(char*)end = '\0'; // make start line null-terminated
        p = strchr(beg, ' ');
        if (!p) return 400;

        static std::unordered_map<fastring, int>* mm = method_map();
        _s.clear();
        _s.append(beg, p - beg).toupper();
        auto it = mm->find(_s);
        if (it != mm->end()) {
            _method = (Method)(it->second);
        } else {
            return 405; // Method Not Allowed
        }

        while (*++p == ' ');
        q = strchr(p, ' ');
        if (!q) return 400;

        _url.append(p, q - p);
        while (*++q == ' ');
        if (*q == '\0') return 400;

        static std::unordered_map<fastring, int>* vm = version_map();
        _s.clear();
        _s.append(q, end - q).toupper();
        it = vm->find(_s);
        if (it != vm->end()) {
            _version = (Version)(it->second);
        } else {
            return 505; // HTTP Version Not Supported
        }
    }

    x += 2; // skip "\r\n"

    { /* parse header */
        parse_headers(x, s, _headers);
    }

    { /* parse body size */
        const char* v = this->header("CONTENT-LENGTH");
        if (*v == '\0' || *v == '0') {
            _body_size = 0;
            return 0;
        } else {
            int n = atoi(v);
            if (n >= 0) {
                if ((uint32)n > FLG_http_max_body_size) return 413;
                _body_size = n;
                return 0;
            }
            ELOG << "http parse error, invalid content-length: " << v;
            return 400;
        }
    }
}

} // http

namespace so {

void easy(const char* root_dir, const char* ip, int port) {
    return so::easy(root_dir, ip, port, NULL, NULL);
}

void easy(const char* root_dir, const char* ip, int port, const char* key, const char* ca) {
    http::Server serv;
    std::vector<LruMap<fastring, std::pair<fastring, int64>>> contents(co::scheduler_num());
    fastring root(root_dir);
    if (root.empty()) root.append('.');

    serv.on_req(
        [&](const http::Req& req, http::Res& res) {
            if (!req.is_method_get()) {
                res.set_status(405);
                return;
            }

            fastring url = path::clean(req.url());
            if (!url.starts_with('/')) {
                res.set_status(403);
                return;
            }

            fastring path = path::join(root, url);
            if (fs::isdir(path)) path = path::join(path, "index.html");

            auto& map = contents[co::scheduler_id()];
            auto it = map.find(path);
            if (it != map.end()) {
                if (now::ms() < it->second.second + 300 * 1000) {
                    res.set_status(200);
                    auto& s = it->second.first;
                    res.set_body(s.data(), s.size());
                    return;
                } else {
                    map.erase(it); // timeout
                }
            }

            fs::file f(path.c_str(), 'r');
            if (!f) {
                res.set_status(404);
                return;
            }

            fastring s = f.read(f.size());
            res.set_status(200);
            res.set_body(s.data(), s.size());
            map.insert(path, std::make_pair(std::move(s), now::ms()));
        }
    );

    if (key && ca && *key && *ca) {
        serv.start(ip, port, key, ca);
    } else{
        serv.start(ip, port);
    }

    while (true) sleep::sec(1024);
}

} // so
