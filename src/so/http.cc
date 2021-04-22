#include "co/so/http.h"
#include "co/so/tcp.h"
#include "co/so/ssl.h"
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

DEF_uint32(http_max_header_size, 4096, "#2 max size of http header");
DEF_uint32(http_max_body_size, 8 << 20, "#2 max size of http body, default: 8M");
DEF_uint32(http_recv_timeout, 1024, "#2 recv timeout in ms");
DEF_uint32(http_send_timeout, 1024, "#2 send timeout in ms");
DEF_uint32(http_conn_timeout, 3000, "#2 connect timeout in ms");
DEF_uint32(http_conn_idle_sec, 180, "#2 connection may be closed if no data was recieved for n seconds");
DEF_uint32(http_max_idle_conn, 128, "#2 max idle connections");
DEF_bool(http_log, true, "#2 enable http log if true");

#define HTTPLOG LOG_IF(FLG_http_log)

namespace so {
namespace http {

class ServerImpl {
  public:
    ServerImpl();
    ~ServerImpl();

    void on_req(std::function<void(const Req&, Res&)>&& f) {
        _on_req = std::move(f);
    }

    void start(const char* ip, int port);
    void start(const char* ip, int port, const char* key, const char* ca);

  private:
    void on_tcp_connection(sock_t fd);
    void on_tcp_connection_v2(sock_t fd);

  #ifdef CO_SSL
    void on_ssl_connection(SSL* s);
    void on_ssl_connection_v2(SSL* s);
  #endif

  private:
    co::Pool _buffer; // buffer for recieving http data
    uint32 _conn_num;
    bool _enable_ssl;
    void* _serv;
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
    ((ServerImpl*)_p)->start(ip, port);
}

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    ((ServerImpl*)_p)->start(ip, port, key, ca);
}

ServerImpl::ServerImpl()
    : _buffer(
          []() { return (void*) new fastring(4096); },
          [](void* p) { delete (fastring*)p; }
      ), _conn_num(0), _enable_ssl(false), _serv(0) {
}

ServerImpl::~ServerImpl() {
    if (_enable_ssl) {
      #ifdef CO_SSL
        delete (ssl::Server*)_serv;
      #endif
    } else {
        delete (tcp::Server*)_serv;
    }
}

void ServerImpl::start(const char* ip, int port) {
    CHECK(_on_req != NULL) << "req callback must be set..";
    tcp::Server* s = new tcp::Server();
    s->on_connection(&ServerImpl::on_tcp_connection, this);
    s->start(ip, port);
    _serv = s;
}

void ServerImpl::start(const char* ip, int port, const char* key, const char* ca) {
#ifdef CO_SSL
    CHECK(_on_req != NULL) << "req callback must be set..";
    _enable_ssl = true;
    ssl::Server* s = new ssl::Server();
    s->on_connection(&ServerImpl::on_ssl_connection, this);
    s->start(ip, port, key, ca);
    _serv = s;
#else
    CHECK(false) << "openssl must be installed to use the https feature";
#endif
}

int parse_req(fastring& s, size_t end, Req* req, int* body_len);
int parse_res(fastring& s, size_t end, Res* res, int* body_len);

void ServerImpl::on_tcp_connection(sock_t fd) {
    char c;
    int r = 0, body_len = 0;
    size_t pos = 0;
    fastring* buf = 0;
    Req req;
    Res res;

    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    r = atomic_inc(&_conn_num);
    DLOG << "http conn num: " << r;

    while (true) {
        {
          recv_beg:
            if (!buf) {
                r = co::recv(fd, &c, 1, FLG_http_conn_idle_sec * 1000);
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

            // try to recv the http header
            while ((pos = buf->find("\r\n\r\n")) == buf->npos) {
                if (buf->size() > FLG_http_max_header_size) goto header_too_long_err;
                buf->reserve(buf->size() + 1024);
                r = co::recv(
                    fd, (void*)(buf->data() + buf->size()), 
                    (int)(buf->capacity() - buf->size()), FLG_http_recv_timeout
                );
                if (r == 0) goto recv_zero_err;
                if (r < 0) goto recv_err;
                buf->resize(buf->size() + r);
            }

            // parse http req
            r = parse_req(*buf, pos, &req, &body_len);
            if (r != 0) {
                fastring s(120);
                s << "HTTP/1.1" << ' ' << r << ' ' << Res::status_str(r) << "\r\n";
                s << "Content-Length: 0" << "\r\n";
                s << "Connection: close" << "\r\n";
                s << "\r\n";
                co::send(fd, s.data(), (int)s.size(), FLG_http_send_timeout);
                HTTPLOG << "http parse req error, send res: " << s;
                goto err_end;
            }

            if ((uint32)body_len > FLG_http_max_body_size) goto body_too_long_err;

            // try to recv the remain part of http body
            {
                const size_t total_len = pos + 4 + body_len;
                if (body_len > 0) {
                    if (buf->size() < total_len) {
                        buf->reserve(total_len);
                        r = co::recvn(
                            fd, (void*)(buf->data() + buf->size()), 
                            (int)(total_len - buf->size()), FLG_http_recv_timeout
                        );
                        if (r == 0) goto recv_zero_err;
                        if (r < 0) goto recv_err;
                        buf->resize(total_len);
                    }
                    req.set_body(buf->substr(pos + 4, body_len));
                }

                if (buf->size() == total_len) {
                    buf->clear();
                    _buffer.push(buf);
                    buf = 0;
                } else {
                    buf->lshift(total_len);
                }
            };
        };

        // handle the http request
        {
            HTTPLOG << "http recv req: " << req.dbg();
            bool need_close = false;
            const fastring& conn = req.header("CONNECTION");
            if (!conn.empty()) res.add_header("Connection", conn);

            if (req.is_version_http10()) {
                res.set_version_http10();
                if (conn.empty() || conn.lower() != "keep-alive") need_close = true;
            } else {
                if (!conn.empty() && conn == "close") need_close = true;
            }

            _on_req(req, res);
            fastring s = res.str();
            r = co::send(fd, s.data(), (int)s.size(), FLG_http_send_timeout);
            if (r < 0) goto send_err;

            s.resize(s.size() - res.body_len());
            HTTPLOG << "http send res: " << s;
            if (need_close) { co::close(fd); goto cleanup; }
        };

        req.clear();
        res.clear();
        body_len = 0;
    }

  recv_zero_err:
    LOG << "http client close the connection: " << co::peer(fd) << ", connfd: " << fd;
    co::close(fd);
    goto cleanup;
  idle_err:
    LOG << "http close idle connection: " << co::peer(fd) << ", connfd: " << fd;
    co::reset_tcp_socket(fd);
    goto cleanup;
  header_too_long_err:
    ELOG << "http recv error: header too long";
    goto err_end;
  body_too_long_err:
    ELOG << "http recv error: body too long: " << body_len;
    goto err_end;
  recv_err:
    ELOG << "http recv error: " << co::strerror();
    goto err_end;
  send_err:
    ELOG << "http send error: " << co::strerror();
    goto err_end;
  err_end:
    co::reset_tcp_socket(fd, 1000);
  cleanup:
    atomic_dec(&_conn_num);
    if (buf) { 
        buf->clear();
        buf->capacity() <= 1024 * 1024 ? _buffer.push(buf) : delete buf;
    }
}

#ifdef CO_SSL
void ServerImpl::on_ssl_connection(SSL* s) {
    char c;
    int r = 0, body_len = 0;
    size_t pos = 0;
    fastring* buf = 0;
    Req req;
    Res res;

    sock_t fd = ssl::get_fd(s);
    if (fd < 0) {
        ELOG << "ssl get_fd failed: " << ssl::strerror(s);
        ssl::free_ssl(s);
        return;
    }

    r = atomic_inc(&_conn_num);
    DLOG << "https conn num: " << r;

    while (true) {
        {
          recv_beg:
            if (!buf) {
                r = ssl::recv(s, &c, 1, FLG_http_conn_idle_sec * 1000);
                if (r == 0) goto recv_zero_err;
                if (r < 0) {
                    if (!ssl::timeout()) goto recv_err;
                    if (_conn_num > FLG_http_max_idle_conn) goto idle_err;
                    goto recv_beg;
                }
                buf = (fastring*) _buffer.pop();
                buf->clear();
                buf->append(c);
            }

            // try to recv the http header
            while ((pos = buf->find("\r\n\r\n")) == buf->npos) {
                if (buf->size() > FLG_http_max_header_size) goto header_too_long_err;
                buf->reserve(buf->size() + 1024);
                r = ssl::recv(
                    s, (void*)(buf->data() + buf->size()), 
                    (int)(buf->capacity() - buf->size()), FLG_http_recv_timeout
                );
                if (r == 0) goto recv_zero_err;
                if (r < 0) goto recv_err;
                buf->resize(buf->size() + r);
            }

            // parse http req
            r = parse_req(*buf, pos, &req, &body_len);
            if (r != 0) {
                fastring fs(120);
                fs << "HTTP/1.1" << ' ' << r << ' ' << Res::status_str(r) << "\r\n";
                fs << "Content-Length: 0" << "\r\n";
                fs << "Connection: close" << "\r\n";
                fs << "\r\n";
                ssl::send(s, fs.data(), (int)fs.size(), FLG_http_send_timeout);
                HTTPLOG << "https parse req error, send res: " << fs;
                goto err_end;
            }

            if ((uint32)body_len > FLG_http_max_body_size) goto body_too_long_err;

            // try to recv the remain part of http body
            {
                const size_t total_len = pos + 4 + body_len;
                if (body_len > 0) {
                    if (buf->size() < total_len) {
                        buf->reserve(total_len);
                        r = ssl::recvn(
                            s, (void*)(buf->data() + buf->size()), 
                            (int)(total_len - buf->size()), FLG_http_recv_timeout
                        );
                        if (r == 0) goto recv_zero_err;
                        if (r < 0) goto recv_err;
                        buf->resize(total_len);
                    }
                    req.set_body(buf->substr(pos + 4, body_len));
                }

                if (buf->size() == total_len) {
                    buf->clear();
                    _buffer.push(buf);
                    buf = 0;
                } else {
                    buf->lshift(total_len);
                }
            };
        };

        // handle the http request
        {
            HTTPLOG << "https recv req: " << req.dbg();
            bool need_close = false;
            const fastring& conn = req.header("CONNECTION");
            if (!conn.empty()) res.add_header("Connection", conn);

            if (req.is_version_http10()) {
                res.set_version_http10();
                if (conn.empty() || conn.lower() != "keep-alive") need_close = true;
            } else {
                if (!conn.empty() && conn == "close") need_close = true;
            }

            _on_req(req, res);
            fastring fs = res.str();
            r = ssl::send(s, fs.data(), (int)fs.size(), FLG_http_send_timeout);
            if (r <= 0) goto send_err;

            fs.resize(fs.size() - res.body_len());
            HTTPLOG << "https send res: " << fs;
            if (need_close) { co::close(fd); goto cleanup; }
        };

        req.clear();
        res.clear();
        body_len = 0;
    }

  recv_zero_err:
    LOG << "https client may close the connection: " << co::peer(fd);
    ssl::shutdown(s);
    co::close(fd);
    goto cleanup;
  idle_err:
    LOG << "https close idle connection: " << co::peer(fd);
    co::reset_tcp_socket(fd);
    goto cleanup;
  header_too_long_err:
    ELOG << "https recv error: header too long";
    goto err_end;
  body_too_long_err:
    ELOG << "https recv error: body too long: " << body_len;
    goto err_end;
  recv_err:
    ELOG << "https recv error: " << ssl::strerror(s);
    goto err_end;
  send_err:
    ELOG << "https send error: " << ssl::strerror(s);
    goto err_end;
  err_end:
    ssl::free_ssl(s);
    co::reset_tcp_socket(fd, 1000);
  cleanup:
    atomic_dec(&_conn_num);
    if (buf) { 
        buf->clear();
        buf->capacity() <= 1024 * 1024 ? _buffer.push(buf) : delete buf;
    }
}
#endif

Client::Client(const char* serv_url) : _https(false) {
    const char* p = serv_url;
    fastring ip;
    int port = 0;

    if (memcmp(serv_url, "https://", 8) == 0) {
        p += 8;
        _https = true;
    } else {
        if (memcmp(serv_url, "http://", 7) == 0) p += 7;
    }

    const char* s = strchr(p, ':');
    if (s != NULL) {
        const char* r = strrchr(p, ':');
        if (r == s) {                  /* ipv4:port */
            ip = fastring(p, r - p);
            port = atoi(r + 1);
        } else if (*(r - 1) == ']') {  /* ipv6:port */
            if (*p == '[') ++p;
            ip = fastring(p, r - p - 1);
            port = atoi(r + 1);
        }
    } 

    if (_https) {
      #ifdef CO_SSL
        _p = new ssl::Client(ip.empty() ? p : ip.c_str(), port == 0 ? 443 : port);
      #endif
    } else {
        _p = new tcp::Client(ip.empty() ? p : ip.c_str(), port == 0 ? 80 : port);
    }
}

Client::~Client() = default;

// user-defined error code:
//   577: "Connection Timeout";
//   578: "Connection Closed";
//   579: "Send Timeout";   580: "Recv Timeout";
//   581: "Send Failed";    582: "Recv Failed";
void Client::call_http(const Req& req, Res& res) {
    tcp::Client* c = (tcp::Client*)_p;
    if (!c->connected() && !c->connect(FLG_http_conn_timeout)) {
        res.set_status(577); // Connection Timeout
        return;
    }

    int r = 0, body_len = 0;
    size_t pos = 0;

    { /* send request */
        fastring s = req.str();
        r = c->send(s.data(), (int)s.size(), FLG_http_send_timeout);
        if (r < 0) goto send_err;
        s.resize(s.size() - req.body_len());
        HTTPLOG << "http send req: " << s;
    };

    { /* recv response */
        fastring buf(4096);
        do {
            if (buf.size() > FLG_http_max_header_size) goto header_too_long_err;
            buf.reserve(buf.size() + 1024);
            r = c->recv(
                (void*)(buf.data() + buf.size()), 
                (int)(buf.capacity() - buf.size()), FLG_http_recv_timeout
            );
            if (r == 0) goto recv_zero_err;
            if (r < 0) goto recv_err;
            buf.resize(buf.size() + r);
        } while ((pos = buf.find("\r\n\r\n")) == buf.npos);

        r = parse_res(buf, pos, &res, &body_len);
        if (r != 0) goto parse_err;
        if ((uint32)body_len > FLG_http_max_body_size) goto body_too_long_err;

        {
            size_t total_len = pos + 4 + body_len;
            if (body_len > 0) {
                if (buf.size() < total_len) {
                    buf.reserve(total_len);
                    r = c->recvn(
                        (void*)(buf.data() + buf.size()), 
                        (int)(total_len - buf.size()), FLG_http_recv_timeout
                    );
                    if (r == 0) goto recv_zero_err;
                    if (r < 0) goto recv_err;
                    buf.resize(total_len);
                }
                res.set_body(buf.substr(pos + 4, body_len));
            }
            if (buf.size() != total_len) goto content_length_err;
        };

        HTTPLOG << "http recv res: " << res.dbg();
        goto success;
    };

  header_too_long_err:
    ELOG << "http recv error: header too long";
    res.set_status(500);
    goto err_end;
  body_too_long_err:
    ELOG << "http recv error: body too long";
    res.set_status(500);
    goto err_end;
  content_length_err:
    ELOG << "http content length error";
    res.set_status(500);
    goto err_end;
  parse_err:
    ELOG << "http parse response error..";
    res.set_status(500);
    goto err_end;
  recv_zero_err:
    ELOG << "http server close the connection..";
    res.set_status(578); // Connection Closed
    goto err_end;
  recv_err:
    ELOG << "http recv error: " << co::strerror();
    res.set_status(co::timeout() ? 580 : 582);
    goto err_end;
  send_err:
    ELOG << "http send error: " << co::strerror();
    res.set_status(co::timeout() ? 579 : 581);
    goto err_end;
  err_end:
    c->disconnect();
  success:
    return;
}

void Client::call_https(const Req& req, Res& res) {
  #ifdef CO_SSL
    ssl::Client* c = (ssl::Client*)_p;
    if (!c->connected() && !c->connect(FLG_http_conn_timeout)) {
        res.set_status(577); // Connection Timeout
        return;
    }

    int r = 0, body_len = 0;
    size_t pos = 0;

    { /* send request */
        fastring s = req.str();
        r = c->send(s.data(), (int) s.size(), FLG_http_send_timeout);
        if (r <= 0) goto send_err;

        s.resize(s.size() - req.body_len());
        HTTPLOG << "https send req: " << s;
    };

    { /* recv response */
        fastring buf(4096);
        do {
            if (buf.size() > FLG_http_max_header_size) goto header_too_long_err;
            buf.reserve(buf.size() + 1024);
            r = c->recv(
                (void*)(buf.data() + buf.size()), 
                (int)(buf.capacity() - buf.size()), FLG_http_recv_timeout
            );
            if (r == 0) goto recv_zero_err;
            if (r == -1) goto recv_err;
            buf.resize(buf.size() + r);
        } while ((pos = buf.find("\r\n\r\n")) == buf.npos);

        r = parse_res(buf, pos, &res, &body_len);
        if (r != 0) goto parse_err;
        if ((uint32)body_len > FLG_http_max_body_size) goto body_too_long_err;

        {
            size_t total_len = pos + 4 + body_len;
            if (body_len > 0) {
                if (buf.size() < total_len) {
                    buf.reserve(total_len);
                    r = c->recvn(
                        (void*)(buf.data() + buf.size()), 
                        (int)(total_len - buf.size()), FLG_http_recv_timeout
                    );
                    if (r == 0) goto recv_zero_err;
                    if (r < 0) goto recv_err;
                    buf.resize(total_len);
                }
                res.set_body(buf.substr(pos + 4, body_len));
            }
            if (buf.size() != total_len) goto content_length_err;
        };

        HTTPLOG << "http recv res: " << res.dbg();
        goto success;
    };

  header_too_long_err:
    ELOG << "https recv error: header too long";
    res.set_status(500);
    goto err_end;
  body_too_long_err:
    ELOG << "https recv error: body too long";
    res.set_status(500);
    goto err_end;
  content_length_err:
    ELOG << "https content length error";
    res.set_status(500);
    goto err_end;
  parse_err:
    ELOG << "https parse response error..";
    res.set_status(500);
    goto err_end;
  recv_zero_err:
    ELOG << "https server close the connection..";
    res.set_status(578); // Connection Closed
    goto err_end;
  recv_err:
    ELOG << "https recv error: " << ssl::strerror(c->ssl());
    res.set_status(ssl::timeout() ? 580 : 582);
    goto err_end;
  send_err:
    ELOG << "https send error: " << ssl::strerror(c->ssl());
    res.set_status(ssl::timeout() ? 579 : 581);
    goto err_end;
  err_end:
    c->disconnect();
  success:
    return;
  #else
    CHECK(false) << "openssl must be installed to use the https feature";
  #endif
}

fastring Req::str() const {
    fastring s;
    s << method_str() << ' ' << _url << ' ' << version_str() << "\r\n";

    if (!_body.empty() && !this->parsing()) {
        s << "Content-Length: " << _body.size() << "\r\n";
    }

    for (size_t i = 0; i < _headers.size(); i += 2) {
        s << _headers[i] << ": " << _headers[i + 1] << "\r\n";
    }

    s << "\r\n";
    if (!_body.empty()) s << _body;
    return std::move(s);
}

fastring Req::dbg() const {
    fastring s;
    s << method_str() << ' ' << _url << ' ' << version_str() << "\r\n";

    if (!_body.empty() && !this->parsing()) {
        s << "Content-Length: " << _body.size() << "\r\n";
    }

    for (size_t i = 0; i < _headers.size(); i += 2) {
        s << _headers[i] << ": " << _headers[i + 1] << "\r\n";
    }

    return std::move(s);
}

fastring Res::str() const {
    fastring s;
    s << version_str() << ' ' << status() << ' ' << status_str() << "\r\n";

    if (!this->parsing()) {
        s << "Content-Length: " << _body.size() << "\r\n";
    }

    for (size_t i = 0; i < _headers.size(); i += 2) {
        s << _headers[i] << ": " << _headers[i + 1] << "\r\n";
    }

    s << "\r\n";
    if (!_body.empty()) s << _body;
    return std::move(s);
}

fastring Res::dbg() const {
    fastring s;
    s << version_str() << ' ' << status() << ' ' << status_str() << "\r\n";

    if (!this->parsing()) {
        s << "Content-Length: " << _body.size() << "\r\n";
    }

    for (size_t i = 0; i < _headers.size(); i += 2) {
        s << _headers[i] << ": " << _headers[i + 1] << "\r\n";
    }

    return std::move(s);
}

int parse_req_start_line(const char* beg, const char* end, Req* req) {
    const char* p = strchr(beg, ' ');
    if (!p || p >= end) return 400; // Bad Request

    fastring method(std::move(fastring(beg, p - beg).toupper()));
    if (method == "GET") {
        req->set_method_get();
    } else if (method == "POST") {
        req->set_method_post();
    } else if (method == "HEAD") {
        req->set_method_head();
    } else if (method == "PUT") {
        req->set_method_put();
    } else if (method == "DELETE") {
        req->set_method_delete();
    } else {
        return 405; // Method Not Allowed
    }
    
    do { ++p; } while (*p == ' ' && p < end);
    const char* q = strchr(p, ' ');
    if (!q || q >= end) return 400;
    req->set_url(fastring(p, q - p));

    do { ++q; } while (*q == ' ' && q < end);
    if (q >= end) return 400;

    fastring version(std::move(fastring(q, end - q).toupper()));
    if (version == "HTTP/1.1") {
        req->set_version_http11();
    } else if (version == "HTTP/1.0") {
        req->set_version_http10();
    } else {
        return 505; // HTTP Version Not Supported
    }

    return 0;
}

int parse_header(fastring& s, size_t beg, size_t end, Base* base, int* body_len) {
    size_t x;
    while (beg < end) {
        x = s.find('\r', beg);
        if (s[x + 1] != '\n') return -1;

        size_t t = s.find(':', beg);
        if (t >= x) return -1;

        fastring key = s.substr(beg, t - beg);
        do { ++t; } while (s[t] == ' ' && t < x);
        base->add_header(std::move(key), s.substr(t, x - t));
        beg = x + 2;
    }

    fastring v = base->header("CONTENT-LENGTH");
    if (v.empty() || v == "0") {
        *body_len = 0;
        return 0;
    } else {
        int len = atoi(v.c_str());
        if (len <= 0) {
            ELOG << "http parse error, invalid content-length: " << v;
            return -1;
        }

        if ((uint32)len > FLG_http_max_body_size) {
            ELOG << "http parse error, content-length is too long: " << v;
            return -1;
        }

        *body_len = len;
        return 0;
    }
}

int parse_req(fastring& s, size_t end, Req* req, int* body_len) {
    req->set_parsing();

    // start line
    size_t p = s.find('\r');
    if (s[p + 1] != '\n') return 400;

    const char* beg = s.c_str();
    int r = parse_req_start_line(beg, beg + p, req);
    if (r != 0) return r;
    p += 2;

    // headers 
    r = parse_header(s, p, end, req, body_len);
    return r == 0 ? 0 : 400;
}

int parse_res_start_line(const char* beg, const char* end, Res* res) {
    const char* p = strchr(beg, ' ');
    if (!p || p >= end) return -1;

    fastring version(std::move(fastring(beg, p - beg).toupper()));
    if (version == "HTTP/1.1") {
        res->set_version_http11();
    } else if (version == "HTTP/1.0") {
        res->set_version_http10();
    }
    
    do { ++p; } while (*p == ' ' && p < end);
    const char* q = strchr(p, ' ');
    if (!q || q >= end) return -1;

    int status = atoi(fastring(p, q - p).c_str());
    res->set_status(status);

    do { ++q; } while (*q == ' ' && q < end);
    if (q >= end) return -1;
    return 0;
}

int parse_res(fastring& s, size_t end, Res* res, int* body_len) {
    res->set_parsing();

    // start line
    size_t p = s.find('\r');
    if (s[p + 1] != '\n') return -1;

    const char* beg = s.c_str();
    int r = parse_res_start_line(beg, beg + p, res);
    if (r != 0) return r;
    p += 2;

    // headers 
    return parse_header(s, p, end, res, body_len);
}

const char** Res::create_status_table() {
    static const char* s[600];
    for (int i = 0; i < 600; ++i) s[i] = "";

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

    // ================================================================
    // user-defined error code: 577 - 599
    // ================================================================
    s[577] = "Connection Timeout";
    s[578] = "Connection Closed";
    s[579] = "Send Timeout";
    s[580] = "Recv Timeout";
    s[581] = "Send Failed";
    s[582] = "Recv Failed";
    return s;
}

} // http

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
                    res.set_body(it->second.first);
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
            map.insert(path, std::make_pair(s, now::ms()));
            res.set_status(200);
            res.set_body(std::move(s));
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
