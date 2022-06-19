#include "./http.h"
#include "co/http.h"
#include "co/tcp.h"
#include "co/co.h"
#include "co/god.h"
#include "co/fastream.h"
#include "co/stl.h"
#include "co/time.h"
#include "co/fs.h"
#include "co/path.h"
#include "co/lru_map.h"

#ifdef HAS_LIBCURL
#include <curl/curl.h>
#endif

DEF_uint32(http_max_header_size, 4096, ">>#2 max size of http header");
DEF_uint32(http_max_body_size, 8 << 20, ">>#2 max size of http body, default: 8M");
DEF_uint32(http_timeout, 3000, ">>#2 send or recv timeout in ms for http client");
DEF_uint32(http_conn_timeout, 3000, ">>#2 connect timeout in ms for http client");
DEF_uint32(http_recv_timeout, 3000, ">>#2 recv timeout in ms for http server");
DEF_uint32(http_send_timeout, 3000, ">>#2 send timeout in ms for http server");
DEF_uint32(http_conn_idle_sec, 180, ">>#2 if a connection was idle for this seconds, the server may reset it");
DEF_uint32(http_max_idle_conn, 128, ">>#2 max idle connections for http server");
DEF_bool(http_log, true, ">>#2 enable http server log if true");

#define HTTPLOG LOG_IF(FLG_http_log)

namespace http {

inline fastring& fastring_cache() {
    static __thread fastring* kS = 0;
    if (kS) return *kS;
    return *(kS = new fastring(128));
}

inline fastream& fastream_cache() {
    return *(fastream*) &fastring_cache();
}

/**
 * ===========================================================================
 * HTTP client 
 *   - libcurl & zlib required. 
 *   - openssl required for https. 
 * ===========================================================================
 */

#ifdef HAS_LIBCURL
struct curl_ctx_t {
    curl_ctx_t() = delete;

    ~curl_ctx_t() {
        if (l) { curl_slist_free_all(l); l = 0; }
        if (easy) { curl_easy_cleanup(easy); easy = 0; }
        if (arr) { co::free(arr, arr_cap << 2); arr = 0; }
    }

    void add_header(uint32 k) {
        if (arr_cap < arr_size + 2) {
            arr = (uint32*) co::realloc(arr, arr_cap << 2, (arr_cap + 32) << 2);
            arr_cap += 32;
            assert(arr);
        }
        arr[arr_size++] = k;
        arr[arr_size++] = 0;
    }

    void clear() {
        header.clear();
        mutable_header.clear();
        body.clear();
        arr_size = 0;
        err[0] = '\0';
    }

    fastring serv_url;
    fastream body;
    fastream header;
    fastream mutable_header;
    uint32* arr; // array of header index
    uint32 arr_size;
    uint32 arr_cap;
    CURL* easy;
    struct curl_slist* l;
    fs::file upfile; // for PUT, the file to upload
    bool header_updated;
    char err[CURL_ERROR_SIZE];
};

static size_t easy_write_cb(void* data, size_t size, size_t count, void* userp);
static size_t easy_read_cb(char* p, size_t size, size_t nmemb, void* userp);
static size_t easy_header_cb(char* p, size_t size, size_t nmemb, void* userp);

void init_easy_opts(CURL* e, curl_ctx_t* ctx) {
    curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(e, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(e, CURLOPT_SSL_VERIFYHOST, 0);

    curl_easy_setopt(e, CURLOPT_HEADERFUNCTION, easy_header_cb);
    curl_easy_setopt(e, CURLOPT_HEADERDATA, (void*)ctx);
    curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, easy_write_cb);
    curl_easy_setopt(e, CURLOPT_WRITEDATA, (void*)ctx);

    curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, FLG_http_conn_timeout);
    curl_easy_setopt(e, CURLOPT_TIMEOUT_MS, FLG_http_timeout);
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

Client::Client(const char* serv_url) : _ctx(0) {
    this->reset(serv_url);
}

Client::~Client() {
    if (_ctx) { _ctx->~curl_ctx_t(); co::free(_ctx, sizeof(*_ctx)); _ctx = 0; }
}

void Client::close() {
    if (_ctx) { _ctx->~curl_ctx_t(); co::free(_ctx, sizeof(*_ctx)); _ctx = 0; }
}

void Client::reset(const char* serv_url) {
    if (_ctx) {
        _ctx->serv_url = serv_url;
    } else {
        static curl_global g;
        _ctx = (curl_ctx_t*) co::zalloc(sizeof(curl_ctx_t));
        _ctx->easy = curl_easy_init();

        auto& s = _ctx->serv_url;
        if (strncmp(serv_url, "https://", 8) == 0 || strncmp(serv_url, "http://", 7) == 0) {
            s.append(serv_url);
        } else {
            const size_t n = strlen(serv_url);
            s.reserve(n + 8);
            s.append("http://").append(serv_url, n); // use http by default
        }
        s.strip('/', 'r'); // remove '/' at the right side

        init_easy_opts(_ctx->easy, _ctx);
    }
}

inline void Client::append_header(const char* s) {
    struct curl_slist* l = curl_slist_append(_ctx->l, s);
    if (l) {
        _ctx->l = l;
        if (!_ctx->header_updated) _ctx->header_updated = true;
    } else {
        ELOG << "curl add header failed: " << s;
    }
}

void Client::add_header(const char* key, const char* val) {
    auto& s = fastream_cache(); s.clear();
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
    auto& s = fastream_cache(); s.clear();
    s << key << ": " << val;
    this->append_header(s.c_str());
}

void Client::remove_header(const char* key) {
    auto& s = fastream_cache(); s.clear();
    s.append(key).append(':');
    this->append_header(s.c_str());
}

inline const char* Client::make_url(const char* url) {
    auto& s = fastream_cache(); s.clear();
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

void Client::put(const char* url, const char* path) {
    _ctx->upfile.open(path, 'r');
    curl_easy_setopt(_ctx->easy, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(_ctx->easy, CURLOPT_URL, make_url(url));
    curl_easy_setopt(_ctx->easy, CURLOPT_READFUNCTION, easy_read_cb);
    curl_easy_setopt(_ctx->easy, CURLOPT_READDATA, (void*)_ctx);
    this->perform();
}

void* Client::easy_handle() const {
    return (void*)_ctx->easy;
}

void Client::perform() {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    _ctx->clear();
    if (_ctx->header_updated) {
        curl_easy_setopt(_ctx->easy, CURLOPT_HTTPHEADER, _ctx->l);
        _ctx->header_updated = false;
    }
    curl_easy_perform(_ctx->easy);
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
    if (_ctx->arr_size == 0) return e;

    fastream& h = _ctx->header;
    fastream& m = _ctx->mutable_header;
    if (m.empty() && !h.empty()) m.append(h.c_str(), h.size() + 1);

    fastring& s = fastring_cache();
    fastring u(key); u.toupper();
    const char* header_begin = m.c_str();
    const char* b;
    const char* p;
    for (uint32 i = 0; i < _ctx->arr_size; i += 2) {
        b = header_begin + _ctx->arr[i];
        p = strchr(b, ':');
        if (p) {
            s.clear();
            s.append(b, p - b).strip(' ').toupper();
            if (s == u) {
                if (_ctx->arr[i + 1] == 0) {
                    b = p;
                    while (*++b == ' ');
                    p = strchr(b, '\r');
                    if (p) *(char*)p = '\0';
                    _ctx->arr[i + 1] = (uint32)(b - header_begin);
                    return b;
                } else {
                    return header_begin + _ctx->arr[i + 1];
                }
            }
        }
    }
    return e;
}

const fastring& Client::header() const {
    return (fastring&)_ctx->header;
}

const fastring& Client::body() const {
    return (fastring&)_ctx->body;
}

size_t easy_write_cb(void* data, size_t size, size_t count, void* userp) {
    curl_ctx_t* ctx = (curl_ctx_t*)userp;
    const size_t n = size * count;
    ctx->body.append(data, n);
    return n;
}

size_t easy_header_cb(char* p, size_t size, size_t nmemb, void* userp) {
    curl_ctx_t* ctx = (curl_ctx_t*)userp;
    const size_t n = size * nmemb;
    fastream& h = ctx->header;

    long code = 0;
    const CURLcode r = curl_easy_getinfo(ctx->easy, CURLINFO_RESPONSE_CODE, &code);
    if (r != CURLE_OK || code == 100) return n;

    if (!h.empty()) {
        if (n > 2) ctx->add_header((uint32)h.size()); // not "\r\n"
        h.append(p, n);
    } else {
        h.append(p, n); // start line
    }
    return n;
}

size_t easy_read_cb(char* p, size_t size, size_t nmemb, void* userp) {
    curl_ctx_t* ctx = (curl_ctx_t*)userp;
    auto& f = ctx->upfile;
    return f ? f.read(p, size * nmemb) : 0;
}

#else
Client::Client(const char*) {
    CHECK(false)
        << "To use http::Client, please build libco with libcurl as follow: \n"
        << "  xmake f --with_libcurl=true\n"
        << "  xmake -v";
}

Client::~Client() {}
void Client::add_header(const char*, const char*) {}
void Client::add_header(const char*, int) {}
void Client::remove_header(const char*) {}
void Client::get(const char*) {}
void Client::head(const char*) {}
void Client::post(const char*, const char*, size_t) {}
void Client::put(const char*, const char*) {}
void Client::del(const char*, const char*, size_t) {}
void Client::set_url(const char*) {}
void* Client::easy_handle() const { return 0; }
void Client::perform() {}
int Client::response_code() const { return 0; }
const char* Client::strerror() const { return ""; }
const char* Client::header(const char*) { return ""; }
const fastring& Client::header() const { static fastring s; return s; }
const fastring& Client::body() const { static fastring s; return s; }
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

static co::hash_map<fastring, int>* create_method_map() {
    static co::hash_map<fastring, int> m;
    m["GET"]     = kGet;
    m["POST"]    = kPost;
    m["HEAD"]    = kHead;
    m["PUT"]     = kPut;
    m["DELETE"]  = kDelete;
    m["OPTIONS"] = kOptions;
    return &m;
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


inline void http_req_t::add_header(uint32 k, uint32 v) {
    if (arr_cap < arr_size + 2) {
        arr = (uint32*) co::realloc(arr, arr_cap << 2, (arr_cap + 32) << 2);
        assert(arr);
        arr_cap += 32;
    }
    arr[arr_size++] = k;
    arr[arr_size++] = v;
}

const char* http_req_t::header(const char* key) const {
    fastring& s = fastring_cache();
    fastring x(key);
    x.toupper();

    for (uint32 i = 0; i < arr_size; i += 2) {
        s.clear();
        s.append(buf->data() + arr[i]).toupper();
        if (s == x) return buf->data() + arr[i + 1];
    }

    static const char* e = "";
    return e;
}


void http_res_t::set_body(const void* s, size_t n) {
    body_size = n;
    if (status == 0) status = 200;
    buf->clear();
    (*buf) << version_str(version) << ' ' << status << ' ' << status_str(status) << "\r\n"
           << "Content-Length: " << n << "\r\n"
           << header << "\r\n";
    buf->append(s, n);
}

const char* Req::header(const char* key) const {
    return _p->header(key);
}

const char* Req::body() const {
    return _p->buf->data() + _p->body;
}

Req::~Req() {
    if (_p) {
        _p->url.~fastring();
        co::free(_p->arr, _p->arr_cap << 2);
        co::free(_p, sizeof(*_p));
        _p = 0;
    }
}

void Res::add_header(const char* k, const char* v) {
    _p->add_header(k, v);
}

void Res::add_header(const char* k, int v) {
    _p->add_header(k, v);
}

void Res::set_body(const void* s, size_t n) {
    _p->set_body(s, n);
}

Res::~Res() {
    if (_p) {
        _p->header.~fastring();
        co::free(_p, sizeof(*_p));
        _p = 0;
    }
}

// @x  beginning of http header
int parse_http_headers(fastring* buf, size_t size, size_t x, http_req_t* req) {
    fastring& m = *buf;
    size_t p, k, v;

    while (x < size) {
        p = m.find('\r', x, size - x); // header end
        if (p == m.npos || m[p + 1] != '\n') return 400;
        m[p] = '\0'; // make value null-terminated

        k = x; // key
        v = m.find(':', x, p - x);
        if (v == m.npos) return 400;
        m[v] = '\0'; // make key null-terminated
        while (m[++v] == ' ');
        req->add_header((uint32)k, (uint32)v);

        x = p + 2;
    }
    return 0;
}

int parse_http_req(fastring* buf, size_t size, http_req_t* req) {
    static co::hash_map<fastring, int>* mm = create_method_map();
    fastring& m = *buf;
    req->buf = buf;

    size_t x = m.find('\r', 0, size); // end of start line
    { /* parse start line: method, url, version */
        if (m[x + 1] != '\n') return 400;

        size_t p, q;
        p = m.find(' ', 0, x);
        if (p == m.npos) return 400;

        auto& s = fastring_cache(); s.clear();
        s.append(m.data(), p).toupper();
        auto it = mm->find(s);
        if (it != mm->end()) {
            req->method = it->second;
        } else {
            return 405; // Method Not Allowed
        }

        while (m[++p] == ' ');
        q = m.find(' ', p, x - p);
        if (q == m.npos) return 400;
        req->url.append(m.data() + p, q - p);

        while (m[++q] == ' ');
        if (m[q] == '\r') return 400;
        s.clear();
        s.append(m.data() + q, x - q).toupper();
        if (s.size() != 8) return 505;
        if (god::byte_eq<uint64>(s.data(), "HTTP/1.1")) {
            req->version = kHTTP11;
        } else if (god::byte_eq<uint64>(s.data(), "HTTP/1.0")) {
            req->version = kHTTP10;
        } else {
            return 505; // HTTP Version Not Supported
        }
    }

    x += 2; // skip "\r\n"

    { /* parse header */
        int r = parse_http_headers(buf, size, x, req);
        if (r != 0) return r;
    }

    { /* parse body size */
        const char* v = req->header("CONTENT-LENGTH");
        if (*v == '\0' || *v == '0') {
            req->body_size = 0;
            return 0;
        } else {
            int n = atoi(v);
            if (n >= 0) {
                if ((uint32)n > FLG_http_max_body_size) return 413;
                req->body_size = n;
                return 0;
            }
            ELOG << "http parse error, invalid content-length: " << v;
            return 400;
        }
    }
}

class ServerImpl {
  public:
    ServerImpl() : _started(false), _stopped(false) {}
    ~ServerImpl() = default;

    void on_req(std::function<void(const Req&, Res&)>&& f) {
        _on_req = std::move(f);
    }

    void start(const char* ip, int port, const char* key, const char* ca);

    void on_connection(tcp::Connection conn);

    void exit() {
        atomic_store(&_stopped, true);
        _serv.exit();
    }

    bool started() const { return _started; }

  private:
    bool _started;
    bool _stopped;
    tcp::Server _serv;
    std::function<void(const Req&, Res&)> _on_req;
};

Server::Server() {
    _p = co::make<ServerImpl>();
}

Server::~Server() {
    if (_p) {
        auto p = (ServerImpl*)_p;
        if (!p->started()) co::del(p);
        _p = 0;
    }
}

Server& Server::on_req(std::function<void(const Req&, Res&)>&& f) {
    ((ServerImpl*)_p)->on_req(std::move(f));
    return *this;
}

void Server::start(const char* ip, int port) {
    ((ServerImpl*)_p)->start(ip, port, NULL, NULL);
}

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    ((ServerImpl*)_p)->start(ip, port, key, ca);
}

void Server::exit() {
    ((ServerImpl*)_p)->exit();
}

void ServerImpl::start(const char* ip, int port, const char* key, const char* ca) {
    CHECK(_on_req != NULL) << "req callback not set..";
    atomic_store(&_started, true, mo_relaxed);
    _serv.on_connection(&ServerImpl::on_connection, this);
    _serv.on_exit([this]() { co::del(this); });
    _serv.start(ip, port, key, ca);
}

inline int hex2int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

void send_error_message(int err, http_res_t* res, void* conn) {
    fastring s(128);
    res->buf = &s;
    res->status = err;
    res->set_body("", 0);
    ((tcp::Connection*)conn)->send(s.data(), (int)s.size(), FLG_http_send_timeout);
    HTTPLOG << "http send res: " << s;
    res->clear();
}

void ServerImpl::on_connection(tcp::Connection conn) {
    char c;
    int r = 0;
    size_t pos = 0, total_len = 0;
    fastring buf;
    Req req; Res res;
    auto& preq = *(http_req_t**) &req;
    auto& pres = *(http_res_t**) &res;

    god::bless_no_bugs();
    while (true) {
        { /* recv http header and body */
          recv_beg:
            if (buf.capacity() == 0) {
                // try to recieve a single byte
                r = conn.recv(&c, 1, FLG_http_conn_idle_sec * 1000);
                if (r == 0) goto recv_zero_err;
                if (r < 0) {
                    if (!co::timeout()) goto recv_err;
                    if (_stopped) { conn.reset(); goto end; } // server stopped
                    if (_serv.conn_num() > FLG_http_max_idle_conn) goto idle_err;
                    goto recv_beg;
                }
                buf.reserve(4096);
                buf.append(c);
            }

            // recv until the entire http header was done. 
            while ((pos = buf.find("\r\n\r\n")) == buf.npos) {
                if (buf.size() > FLG_http_max_header_size) goto header_too_long_err;
                buf.reserve(buf.size() + 1024);
                r = conn.recv(
                    (void*)(buf.data() + buf.size()), 
                    (int)(buf.capacity() - buf.size()), FLG_http_recv_timeout
                );
                if (r == 0) goto recv_zero_err;
                if (r < 0) {
                    if (!co::timeout()) goto recv_err;
                    if (_serv.conn_num() > FLG_http_max_idle_conn) goto idle_err;
                    if (buf.empty()) { buf.reset(); goto recv_beg; }
                    goto recv_err;
                }
                buf.resize(buf.size() + r);
            }

            buf[pos + 2] = '\0'; // make header null-terminated
            HTTPLOG << "http recv req: " << buf.data();

            // parse http header
            if (preq == 0) preq = (http_req_t*) co::zalloc(sizeof(http_req_t));
            if (pres == 0) pres = (http_res_t*) co::zalloc(sizeof(http_res_t));

            r = parse_http_req(&buf, pos + 2, preq);
            if (r != 0) { /* parse error */
                pres->version = kHTTP11;
                goto parse_err;
            } else {
                pres->version = preq->version;
            }

            // try to recv the remain part of http body
            preq->body = (uint32)(pos + 4); // beginning of http body
            if (preq->body_size > 0) {
                total_len = pos + 4 + preq->body_size;
                if (buf.size() < total_len) {
                    buf.reserve(total_len);
                    r = conn.recvn(
                        (void*)(buf.data() + buf.size()), 
                        (int)(total_len - buf.size()), FLG_http_recv_timeout
                    );
                    if (r == 0) goto recv_zero_err;
                    if (r < 0) goto recv_err;
                    buf.resize(total_len);
                }
                goto handle_req;

            } else {
                const char* const te = preq->header("Transfer-Encoding");
                if (!*te) {
                    total_len = pos + 4;
                    goto handle_req; // no Transfer-Encoding
                }
                if (strcmp(te, "chunked") != 0) { /* Transfer-Encoding is not "chunked" */
                    send_error_message(501, pres, &conn);
                    goto reset_conn;
                }
            }

            { /* chunked Transfer-Encoding */
                // see https://datatracker.ietf.org/doc/html/rfc2616#section-3.6.1
                const bool expect_100_continue = strcmp(preq->header("Expect"), "100-continue") == 0;
                size_t x, o, i, n = 0;
                const size_t hlen = pos + 4; // header length
                fastring s(128);

                if (buf.size() > hlen) {
                    s.append(buf.data() + hlen, buf.size() - hlen);
                    buf.resize(hlen);
                }

                while (true) { /* loop for recving chunked data */
                    while ((x = s.find("\r\n")) == s.npos) {
                        if (expect_100_continue) { /* send 100 continue */
                            send_error_message(100, pres, &conn);
                        }
                        s.reserve(s.size() + 32);
                        r = conn.recv((void*)(s.data() + s.size()), 32, FLG_http_recv_timeout);
                        if (r == 0) goto recv_zero_err;
                        if (r < 0) goto recv_err;
                        s.resize(s.size() + r);
                    }

                    if (x == 0) { s.lshift(2); continue; }

                    // chunked data:  1a[;xxx]\r\ndata\r\n
                    if ((o = s.find(';', 0, x)) == s.npos) o = x;
                    for (i = 0, n = 0; i < o; ++i) {
                        if ((r = hex2int(s[i])) < 0) goto chunk_err;
                        n = (n << 4) + r;
                    }

                    if (n > 0) {
                        if (unlikely(n > FLG_http_max_body_size)) goto body_too_long_err;

                        o = s.size() - x - 2;
                        if (o < n) {
                            buf.append(s.data() + x + 2, o);
                            buf.reserve(buf.size() + n - o + 2);
                            r = conn.recvn((void*)(buf.data() + buf.size()), (int)(n - o + 2), FLG_http_recv_timeout);
                            if (r == 0) goto recv_zero_err;
                            if (r < 0) goto recv_err;
                            buf.resize(buf.size() + r - 2);
                            s.clear();
                        } else {
                            buf.append(s.data() + x + 2, n);
                            s.lshift(s.size() >= x + 4 + n ? x + 4 + n : x + 2 + n);
                        }

                        if (buf.size() - hlen > FLG_http_max_body_size) goto body_too_long_err;

                    } else { /* n == 0, end of chunked data */
                        preq->body_size = (uint32)(buf.size() - hlen);
                        s.lshift(x);
                        while ((x = s.find("\r\n\r\n")) == s.npos) {
                            s.reserve(s.size() + 32);
                            r = conn.recv((void*)(s.data() + s.size()), 32, FLG_http_recv_timeout);
                            if (r == 0) goto recv_zero_err;
                            if (r < 0) goto recv_err;
                            s.resize(s.size() + r);
                        }

                        s[x + 2] = '\0';
                        if (s.size() == 4) {
                            total_len = buf.size();
                        } else { /* \r\n tailing headers \r\n\r\n*/
                            // there are some tailing headers following the chunked data
                            total_len = buf.size() + x + 4;
                            o = buf.size();
                            if (s[0] == '\r' && s[1] == '\n') o += 2;
                            buf.append(s.data(), s.size());
                            r = parse_http_headers(&buf, total_len - 2,  o, preq);
                            if (r != 0) goto parse_err;
                        }

                        break; // exit the chunked loop
                    }
                }
            };
        };

      handle_req:
        { /* handle the http request */
            bool need_close = false;
            fastring s(4096);
            s.append(preq->header("Connection"));
            if (!s.empty()) pres->add_header("Connection", s.c_str());

            if (preq->version != kHTTP10) {
                if (!s.empty() && s == "close") need_close = true;
            } else {
                if (s.empty() || s.tolower() != "keep-alive") need_close = true;
            }

            s.clear();
            pres->buf = &s;
            _on_req(req, res);
            if (s.empty()) pres->set_body("", 0);

            r = conn.send(s.data(), (int)s.size(), FLG_http_send_timeout);
            if (r <= 0) goto send_err;

            s.resize(s.size() - pres->body_size);
            HTTPLOG << "http send res: " << s;
            if (need_close) { conn.close(); goto end; }
        };

        if (buf.size() == total_len) {
            buf.clear();
        } else {
            buf.lshift(total_len);
        }

        preq->clear();
        pres->clear();
        total_len = 0;
        if (_stopped) goto reset_conn;
    }

  recv_zero_err:
    LOG << "http client close the connection: " << co::peer(conn.socket()) << ", connfd: " << conn.socket();
    conn.close();
    goto end;
  idle_err:
    LOG << "http close idle connection: " << co::peer(conn.socket()) << ", connfd: " << conn.socket();
    conn.reset();
    goto end;
  header_too_long_err:
    ELOG << "http recv error: header too long";
    goto reset_conn;
  body_too_long_err:
    send_error_message(413, pres, &conn);
    goto reset_conn;
  parse_err:
    ELOG << "http parse error: " << r;
    send_error_message(r, pres, &conn);
    goto reset_conn;
  recv_err:
    ELOG << "http recv error: " << conn.strerror() << ", sock: " << conn.socket();
    goto reset_conn;
  send_err:
    ELOG << "http send error: " << conn.strerror() << ", sock: " << conn.socket();
    goto reset_conn;
  chunk_err:
    ELOG << "http invalid chunked data..";
    goto reset_conn;
  reset_conn:
    conn.reset(3000);
  end:
    god::bless_no_bugs();
}

} // http

namespace so {

void easy(const char* root_dir, const char* ip, int port) {
    return so::easy(root_dir, ip, port, NULL, NULL);
}

void easy(const char* root_dir, const char* ip, int port, const char* key, const char* ca) {
    http::Server serv;
    typedef LruMap<fastring, std::pair<fastring, int64>> Map;
    co::vector<Map> contents(co::scheduler_num());
    fastring root(path::clean(root_dir));

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

#undef HTTPLOG
