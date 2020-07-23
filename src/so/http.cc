#include "co/so/http.h"
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

DEF_int32(http_max_header_size, 4096, "#2 max size of http header");
DEF_int32(http_max_body_size, 8 << 20, "#2 max size of http body, default: 8M");
DEF_int32(http_recv_timeout, 1024, "#2 recv timeout in ms");
DEF_int32(http_send_timeout, 1024, "#2 send timeout in ms");
DEF_int32(http_conn_timeout, 3000, "#2 connect timeout in ms");
DEF_int32(http_conn_idle_sec, 180, "#2 connection may be closed if no data was recieved for n seconds");
DEF_int32(http_max_idle_conn, 128, "#2 max idle connections");
DEF_bool(http_log, true, "#2 enable http log if true");

#define HTTPLOG LOG_IF(FLG_http_log)

namespace so {
namespace http {

Server::Server(const char* ip, int port)
    : tcp::Server(ip, port), _conn_num(0), _buffer(
          []() { return (void*) new fastring(4096); },
          [](void* p) { delete (fastring*)p; }
      ) {
}

Server::~Server() = default;

void Server::start() {
    tcp::Server::start();
    LOG << "http server start, ip: " << _ip << ", port: " << _port;
}

int parse_req(fastring& s, size_t end, Req* req, int* body_len);
int parse_res(fastring& s, size_t end, Res* res, int* body_len);

void Server::on_connection(Connection* conn) {
    std::unique_ptr<Connection> x(conn);
    sock_t fd = conn->fd;
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    
    LOG << "http server accept new connection: " << *conn << ", conn fd: " << fd
        << ", conn num: " << atomic_inc(&_conn_num);

    char c;
    int r = 0, body_len = 0;
    size_t pos = 0;
    fastring* buf = 0;
    Req req;
    Res res;

    while (true) {
        do {
          recv_beg:
            if (!buf) {
                r = co::recv(fd, &c, 1, FLG_http_conn_idle_sec * 1000);
                if (unlikely(r == 0)) goto recv_zero_err;
                if (unlikely(r == -1)) {
                    if (co::error() != ETIMEDOUT) goto recv_err;
                    if (_conn_num > FLG_http_max_idle_conn) goto idle_err;
                    goto recv_beg;
                }

                buf = (fastring*) _buffer.pop();
                buf->clear();
                buf->append(c);
            }

            while ((pos = buf->find("\r\n\r\n")) == buf->npos) {
                if (buf->size() > FLG_http_max_header_size) goto header_too_long_err;

                buf->reserve(buf->size() + 1024);
                r = co::recv(
                    fd, (void*)(buf->data() + buf->size()), 
                    (int)(buf->capacity() - buf->size()), FLG_http_recv_timeout
                );

                if (unlikely(r == 0)) goto recv_zero_err;
                if (unlikely(r == -1)) goto recv_err;
                buf->resize(buf->size() + r);
            }

            // parse http req
            r = parse_req(*buf, pos, &req, &body_len);
            if (r != 0) {
                fastring s;
                // Use HTTP 1.1 when parse req failed.
                s << "HTTP/1.1" << ' ' << r << ' ' << Res::status_str(r) << "\r\n";
                s << "Content-Length: 0" << "\r\n";
                s << "Connection: close" << "\r\n";
                s << "\r\n";
                co::send(fd, s.data(), (int)s.size(), FLG_http_send_timeout);
                HTTPLOG << "http parse req error, send res: " << s;
                goto err_end;
            }

            do {
                size_t total_len = pos + 4 + body_len;

                if (body_len > 0) {
                    if (buf->size() < total_len) {
                        buf->reserve(total_len);
                        r = co::recvn(
                            fd, (void*)(buf->data() + buf->size()), 
                            (int)(total_len - buf->size()), FLG_http_recv_timeout
                        );

                        if (unlikely(r == 0)) goto recv_zero_err;
                        if (unlikely(r == -1)) goto recv_err;
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
            } while (0);
        } while (0);

        res.set_version(Version(req.version()));

        do {
            HTTPLOG << "http recv req: " << req.dbg();
            this->process(req, res);
            fastring s = res.str();
            r = co::send(fd, s.data(), (int) s.size(), FLG_http_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            s.resize(s.size() - res.body_len());
            HTTPLOG << "http send res: " << s;

            if (req.header("CONNECTION") == "close") {
                co::close(fd);
                goto cleanup;
            }
        } while (0);

        req.clear();
        res.clear();
        body_len = 0;
    }

  recv_zero_err:
    LOG << "http client close the connection: " << *conn << " fd: " << fd;
    co::close(fd);
    goto cleanup;
  idle_err:
    ELOG << "http close idle connection: " << *conn;
    co::close(fd);
    goto cleanup;
  header_too_long_err:
    ELOG << "http recv error: header too long";
    goto err_end;
  recv_err:
    ELOG << "http recv error: " << co::strerror();
    goto err_end;
  send_err:
    ELOG << "http send error: " << co::strerror();
    goto err_end;
  err_end:
    co::close(fd, 1000);
  cleanup:
    atomic_dec(&_conn_num);
    if (buf) { 
        buf->clear();
        buf->capacity() <= 1024 * 1024 ? _buffer.push(buf) : delete buf;
    }
}

Client::Client(const char* serv_ip, int serv_port)
    : tcp::Client(serv_ip, serv_port) {
}

Client::~Client() = default;

// user-defined error code:
//   577: "Connection Timeout";
//   578: "Connection Closed";
//   579: "Send Timeout";   580: "Recv Timeout";
//   581: "Send Failed";    582: "Recv Failed";
void Client::call(const Req& req, Res& res) {
    if (!this->connected() && !this->connect(FLG_http_conn_timeout)) {
        res.set_status(577); // Connection Timeout
        return;
    }

    int r = 0, body_len = 0;
    size_t pos = 0;

    // send request
    do {
        fastring s = req.str();
        r = this->send(s.data(), (int) s.size(), FLG_http_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        s.resize(s.size() - req.body_len());
        HTTPLOG << "http send req: " << s;
    } while (0);

    // recv response
    do {
        fastring buf(4096);

        // recv http header
        do {
            if (buf.size() > FLG_http_max_header_size) goto header_too_long_err;

            buf.reserve(buf.size() + 1024);
            r = this->recv(
                (void*)(buf.data() + buf.size()), 
                (int)(buf.capacity() - buf.size()), FLG_http_recv_timeout
            );

            if (unlikely(r == 0)) goto recv_zero_err;
            if (unlikely(r == -1)) goto recv_err;
            buf.resize(buf.size() + r);
        } while ((pos = buf.find("\r\n\r\n")) == buf.npos);

        r = parse_res(buf, pos, &res, &body_len);
        if (r != 0) goto parse_err;

        do {
            size_t total_len = pos + 4 + body_len;

            if (body_len > 0) {
                if (buf.size() < total_len) {
                    buf.reserve(total_len);
                    r = this->recvn(
                        (void*)(buf.data() + buf.size()), 
                        (int)(total_len - buf.size()), FLG_http_recv_timeout
                    );

                    if (unlikely(r == 0)) goto recv_zero_err;
                    if (unlikely(r == -1)) goto recv_err;
                    buf.resize(total_len);
                }

                res.set_body(buf.substr(pos + 4, body_len));
            }

            if (buf.size() != total_len) goto content_length_err;
        } while (0);

        HTTPLOG << "http recv res: " << res.dbg();
        goto success;
    } while (0);

  header_too_long_err:
    ELOG << "http recv error: header too long";
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
    res.set_status(co::error() == ETIMEDOUT ? 580 : 582);
    goto err_end;
  send_err:
    ELOG << "http send error: " << co::strerror();
    res.set_status(co::error() == ETIMEDOUT ? 579 : 581);
    goto err_end;
  err_end:
    this->disconnect();
  success:
    return;
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

        if (len > FLG_http_max_body_size) {
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
    http::Server serv(ip, port);
    std::vector<LruMap<fastring, std::pair<fastring, int64>>> contents(co::max_sched_num());
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

            auto& map = contents[co::sched_id()];
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

    serv.start();
    while (true) sleep::sec(1024);
}

} // so
