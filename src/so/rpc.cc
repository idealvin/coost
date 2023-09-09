#include "./http.h"
#include "co/http.h"
#include "co/rpc.h"
#include "co/tcp.h"
#include "co/co.h"
#include "co/mem.h"
#include "co/fastring.h"
#include "co/fastream.h"
#include "co/str.h"
#include "co/time.h"
#include "co/hash.h"

DEF_int32(rpc_max_msg_size, 8 << 20, ">>#2 max size of rpc message, default: 8M");
DEF_int32(rpc_recv_timeout, 3000, ">>#2 recv timeout in ms");
DEF_int32(rpc_send_timeout, 3000, ">>#2 send timeout in ms");
DEF_int32(rpc_conn_timeout, 3000, ">>#2 connect timeout in ms");
DEF_int32(rpc_conn_idle_sec, 180, ">>#2 connection may be closed if no data was recieved for n seconds");
DEF_int32(rpc_max_idle_conn, 128, ">>#2 max idle connections");
DEF_bool(rpc_log, true, ">>#2 enable rpc log if true");
DEC_uint32(http_max_header_size);

#define RPCLOG LOG_IF(FLG_rpc_log)

namespace rpc {

struct Header {
    uint16 flags; // reserved, 0
    uint16 magic; // 0x7777
    uint32 len;   // body len
}; // 8 bytes

static const uint16 kMagic = 0x7777;

inline void set_header(const void* header, uint32 msg_len) {
    ((Header*)header)->flags = 0;
    ((Header*)header)->magic = kMagic;
    ((Header*)header)->len = hton32(msg_len);
}

class ServerImpl {
  public:
    static void ping(json::Json&, json::Json& res) {
        res.add_member("res", "pong");
    }

    ServerImpl() : _started(false), _stopped(false) {
        using std::placeholders::_1;
        using std::placeholders::_2;
        _methods["ping"] = &ServerImpl::ping;
    }

    ~ServerImpl() = default;

    void add_service(const std::shared_ptr<Service>& s) {
        _services[s->name()] = s;
        for (auto& x : s->methods()) {
            _methods[x.first] = x.second;
        }
    }

    Service::Fun* find_method(const char* name) {
        auto it = _methods.find(name);
        return it != _methods.end() ? &it->second : nullptr;
    }

    void on_connection(tcp::Connection conn);

    void start(const char* ip, int port, const char* url, const char* key, const char* ca) {
        _url = url;
        atomic_store(&_started, true, mo_relaxed);
        _tcp_serv.on_connection(&ServerImpl::on_connection, this);
        _tcp_serv.on_exit([this]() { co::del(this); });
        _tcp_serv.start(ip, port, key, ca);
    }

    bool started() const { return _started; }

    void exit() {
        atomic_store(&_stopped, true, mo_relaxed);
        _tcp_serv.exit();
    }

    void process(json::Json& req, json::Json& res);

  private:
    tcp::Server _tcp_serv;
    bool _started;
    bool _stopped;
    co::hash_map<const char*, std::shared_ptr<Service>> _services;
    co::hash_map<const char*, Service::Fun> _methods;
    fastring _url;
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

Server& Server::add_service(const std::shared_ptr<Service>& s) {
    ((ServerImpl*)_p)->add_service(s);
    return *this;
}

void Server::start(const char* ip, int port, const char* url, const char* key, const char* ca) {
    ((ServerImpl*)_p)->start(ip, port, url, key, ca);
}

void Server::exit() {
    ((ServerImpl*)_p)->exit();
}

void ServerImpl::process(json::Json& req, json::Json& res) {
    auto& x = req.get("api");
    if (x.is_string()) {
        auto m = this->find_method(x.as_c_str());
        if (m) {
            (*m)(req, res);
        } else {
            res.add_member("error", "api not found");
        }
    } else {
        res.add_member("error", "bad req: no string filed 'api'");
    }
}

using http::http_req_t;
using http::http_res_t;

void ServerImpl::on_connection(tcp::Connection conn) {
    int kind = 0; // 0: init, 1: RPC, 2: HTTP
    int r = 0, len = 0;
    union {
        Header header;
        char c;
    };
    fastring buf;
    json::Json req, res;

    size_t pos = 0, total_len = 0;
    http_req_t* preq = 0; 
    http_res_t* pres = 0; 

    while (true) {
        switch (kind) {
          case 1:  goto rpc;
          case 2:  goto http;
          default: goto init;
        }
    
      init:
        r = conn.recvn(&header, sizeof(header), FLG_rpc_conn_idle_sec * 1000);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r < 0)) goto recv_err;
        buf.reserve(4096);

        // if the first 4 bytes is "POST", it is a HTTP request, otherwise it is a RPC request
        if (*(uint32*)&header != *(uint32*)"POST") goto rpc;
        goto http;

      rpc:
        do {
          recv_rpc_beg:
            // recv req from the client
            if (kind == 1) {
                r = conn.recvn(&header, sizeof(header), FLG_rpc_conn_idle_sec * 1000);
                if (unlikely(r == 0)) goto recv_zero_err;
                if (unlikely(r < 0)) {
                    if (!co::timeout()) goto recv_err;
                    if (_stopped) { conn.reset(); goto end; } // server stopped
                    if (_tcp_serv.conn_num() > FLG_rpc_max_idle_conn) goto idle_err;
                    buf.reset();
                    goto recv_rpc_beg;
                }
            } else {
                kind = 1;
            }

            if (unlikely(header.magic != kMagic)) goto magic_err;

            len = ntoh32(header.len);
            if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

            if (buf.capacity() == 0) buf.reserve(4096);
            buf.resize(len);
            r = conn.recvn((char*)buf.data(), len, FLG_rpc_recv_timeout);
            if (unlikely(r == 0)) goto recv_zero_err;
            if (unlikely(r < 0)) goto recv_err;

            req = json::parse(buf.data(), buf.size());
            if (req.is_null()) goto json_parse_err;
            RPCLOG << "rpc recv req: " << req;

            // call rpc and send response to the client
            res.reset();
            this->process(req, res);

            buf.resize(sizeof(Header));
            res.str(buf);
            set_header(buf.data(), (uint32)(buf.size() - sizeof(Header)));
            
            r = conn.send(buf.data(), (int)buf.size(), FLG_rpc_send_timeout);
            if (unlikely(r <= 0)) goto send_err;
            RPCLOG << "rpc send res: " << res;

            if (_stopped) goto reset_conn;
            goto recv_rpc_beg;
        } while (0);

      http:
        do {
          recv_http_beg:
            if (kind == 2) {
                if (buf.capacity() == 0) {
                    // try to recieve a single byte
                    r = conn.recv(&c, 1, FLG_rpc_conn_idle_sec * 1000);
                    if (r == 0) goto recv_zero_err;
                    if (r < 0) {
                        if (!co::timeout()) goto recv_err;
                        if (_stopped) { conn.reset(); goto end; } // server stopped
                        if (_tcp_serv.conn_num() > FLG_rpc_max_idle_conn) goto idle_err;
                        goto recv_http_beg;
                    }
                    buf.reserve(4096);
                    buf.append(c);
                }
            } else {
                kind = 2;
                buf.append(&header, sizeof(header));
            }

            // recv until the entire http header was done. 
            while ((pos = buf.find("\r\n\r\n")) == buf.npos) {
                if (buf.size() > FLG_http_max_header_size) goto header_too_long_err;
                buf.reserve(buf.size() + 1024);
                r = conn.recv(
                    (void*)(buf.data() + buf.size()), 
                    (int)(buf.capacity() - buf.size()), FLG_rpc_recv_timeout
                );
                if (r == 0) goto recv_zero_err;
                if (r < 0) {
                    if (!co::timeout()) goto recv_err;
                    if (_tcp_serv.conn_num() > FLG_rpc_max_idle_conn) goto idle_err;
                    if (buf.empty()) { buf.reset(); goto recv_http_beg; }
                    goto recv_err;
                }
                buf.resize(buf.size() + r);
            }

            buf[pos + 2] = '\0'; // make header null-terminated
            RPCLOG << "rpc recv http header: " << buf.data();

            // parse http header
            if (preq == 0) preq = (http_req_t*) co::zalloc(sizeof(http_req_t));
            if (pres == 0) pres = (http_res_t*) co::zalloc(sizeof(http_res_t));

            r = http::parse_http_req(&buf, pos + 2, preq);
            if (r != 0) { /* parse error */
                pres->version = http::kHTTP11;
                goto http_parse_err;
            } else {
                pres->version = preq->version;
            }

            if (preq->method != http::kPost) {
                send_error_message(405, pres, &conn);
                goto reset_conn;
            }

            if (preq->url != _url) {
                send_error_message(403, pres, &conn);
                goto reset_conn;
            }

            // try to recv the remain part of http body
            preq->body = (uint32)(pos + 4); // beginning of http body
            total_len = pos + 4 + preq->body_size;
            if (preq->body_size > 0) {
                if (buf.size() < total_len) {
                    buf.reserve(total_len);
                    r = conn.recvn(
                        (void*)(buf.data() + buf.size()), 
                        (int)(total_len - buf.size()), FLG_rpc_recv_timeout
                    );
                    if (r == 0) goto recv_zero_err;
                    if (r < 0) goto recv_err;
                    buf.resize(total_len);
                }
            } else {
                // 411 Content-Length required
                send_error_message(411, pres, &conn);
                goto reset_conn;
            }

            { /* handle the http request */
                bool need_close = false;
                fastring x;
                fastring s(4096);
                s.append(preq->header("Connection"));
                if (!s.empty()) pres->add_header("Connection", s.c_str());

                if (preq->version != http::kHTTP10) {
                    if (!s.empty() && s == "close") need_close = true;
                } else {
                    if (s.empty() || s.tolower() != "keep-alive") need_close = true;
                }

                s.clear();
                pres->buf = &s;

                req = json::parse(preq->buf->data() + preq->body, preq->body_size);
                if (req.is_null()) goto json_parse_err;
                RPCLOG << "rpc recv http body: " << req;

                res.reset();
                this->process(req, res);

                x = res.str();
                pres->status = 200;
                pres->add_header("Content-Type", "application/json");
                pres->set_body(x.data(), x.size());

                r = conn.send(s.data(), (int)s.size(), FLG_rpc_send_timeout);
                if (r <= 0) goto send_err;

                RPCLOG << "rpc send http res: " << s;
                if (need_close) { conn.close(); goto end; }
            }

            if (buf.size() == total_len) {
                buf.clear();
            } else {
                buf.trim(total_len, 'l');
            }

            preq->clear();
            pres->clear();
            total_len = 0;
            if (_stopped) goto reset_conn;
            goto recv_http_beg;
        } while (0);
    }

  recv_zero_err:
    LOG << "rpc client close the connection, connfd: " << conn.socket();
    conn.close();
    goto end;
  idle_err:
    ELOG << "rpc close idle connection, connfd: " << conn.socket();
    conn.reset();
    goto end;
  magic_err:
    ELOG << "rpc recv error: bad magic number";
    goto reset_conn;
  header_too_long_err:
    ELOG << "rpc recv error: http header too long";
    goto reset_conn;
  msg_too_long_err:
    ELOG << "rpc recv error: body too long: " << len;
    goto reset_conn;
  recv_err:
    ELOG << "rpc recv error: " << conn.strerror();
    goto reset_conn;
  send_err:
    ELOG << "rpc send error: " << conn.strerror();
    goto reset_conn;
  json_parse_err:
    ELOG << "rpc json parse error: " << buf;
    goto reset_conn;
  http_parse_err:
    ELOG << "rpc http parse error: " << r;
    http::send_error_message(r, pres, &conn);
    goto reset_conn;
  reset_conn:
    conn.reset(3000);
  end:
    if (preq) co::free(preq, sizeof(*preq));
    if (pres) co::free(pres, sizeof(*pres));
}

class ClientImpl {
  public:
    ClientImpl(const char* ip, int port, bool use_ssl)
        : _tcp_cli(ip, port, use_ssl) {
    }

    ClientImpl(const ClientImpl& c)
        : _tcp_cli(c._tcp_cli) {
    }

    ~ClientImpl() = default;

    void call(const json::Json& req, json::Json& res);

    void close() {
        _tcp_cli.disconnect();
    }

  private:
    tcp::Client _tcp_cli;
    fastream _fs;

    bool connect();
};

Client::Client(const char* ip, int port, bool use_ssl) {
    _p = co::make<ClientImpl>(ip, port, use_ssl);
}

Client::Client(const Client& c) {
    _p = co::make<ClientImpl>(*(ClientImpl*)c._p);
}

Client::~Client() {
    co::del((ClientImpl*)_p);
}

void Client::call(const json::Json& req, json::Json& res) {
    return ((ClientImpl*)_p)->call(req, res);
}

void Client::close() {
    return ((ClientImpl*)_p)->close();
}

void Client::ping() {
    json::Json req({{"api", "ping"}}), res;
    this->call(req, res);
}

bool ClientImpl::connect() {
    return _tcp_cli.connect(FLG_rpc_conn_timeout);
}

void ClientImpl::call(const json::Json& req, json::Json& res) {
    int r = 0, len = 0;
    Header header;
    if (!_tcp_cli.connected() && !this->connect()) return;

    // send request
    do {
        _fs.resize(sizeof(Header));
        req.str(_fs);
        set_header((void*)_fs.data(), (uint32)(_fs.size() - sizeof(Header)));

        r = _tcp_cli.send(_fs.data(), (int)_fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r <= 0)) goto send_err;

        RPCLOG << "rpc send req: " << req;
    } while (0);

    // wait for response
    do {
        r = _tcp_cli.recvn(&header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r < 0)) goto recv_err;
        if (unlikely(header.magic != kMagic)) goto magic_err;

        len = ntoh32(header.len);
        if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

        _fs.resize(len);
        r = _tcp_cli.recvn((char*)_fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r < 0)) goto recv_err;

        res = json::parse(_fs.c_str(), _fs.size());
        if (res.is_null()) goto json_parse_err;
        RPCLOG << "rpc recv res: " << res;
        return;
    } while (0);

  magic_err:
    ELOG << "rpc recv error: bad magic number: " << header.magic;
    goto err_end;
  msg_too_long_err:
    ELOG << "rpc recv error: body too long: " << len;
    goto err_end;
  recv_zero_err:
    ELOG << "rpc server close the connection..";
    goto err_end;
  recv_err:
    ELOG << "rpc recv error: " << _tcp_cli.strerror();
    goto err_end;
  send_err:
    ELOG << "rpc send error: " << _tcp_cli.strerror();
    goto err_end;
  json_parse_err:
    ELOG << "rpc json parse error: " << _fs;
    goto err_end;
  err_end:
    _tcp_cli.disconnect();
}

} // rpc
