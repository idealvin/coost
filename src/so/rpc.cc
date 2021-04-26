#include "co/so/rpc.h"
#include "co/so/tcp.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/fastring.h"
#include "co/fastream.h"
#include "co/str.h"
#include "co/hash/md5.h"
#include "co/time.h"
#include <memory>

DEF_int32(rpc_max_msg_size, 8 << 20, "#2 max size of rpc message, default: 8M");
DEF_int32(rpc_recv_timeout, 1024, "#2 recv timeout in ms");
DEF_int32(rpc_send_timeout, 1024, "#2 send timeout in ms");
DEF_int32(rpc_conn_timeout, 3000, "#2 connect timeout in ms");
DEF_int32(rpc_conn_idle_sec, 180, "#2 connection may be closed if no data was recieved for n seconds");
DEF_int32(rpc_max_idle_conn, 128, "#2 max idle connections");
DEF_bool(rpc_log, true, "#2 enable rpc log if true");

#define RPCLOG LOG_IF(FLG_rpc_log)

namespace rpc {

struct Header {
    uint16 info;  // reserved
    uint16 magic; // 0x7777
    uint32 len;   // body len
}; // 8 bytes

static const uint16 kMagic = 0x7777;

inline void set_header(void* header, int msg_len) {
    ((Header*) header)->magic = kMagic;
    ((Header*) header)->len = hton32(msg_len);
}

class ServerImpl {
  public:
    ServerImpl()
        : _conn_num(0), _buffer(
              []() { return (void*) new fastring(4096); },
              [](void* p) { delete (fastring*)p; }
          ) {
    }

    ~ServerImpl() = default;

    void add_service(Service* service) {
        _service.reset(service);
    }

    void on_connection(sock_t fd);

    void start(const char* ip, int port, const char* passwd) {
        if (passwd && *passwd) _passwd = md5sum(passwd);
        _tcp_serv.on_connection(&ServerImpl::on_connection, this);
        _tcp_serv.start(ip, port);
    }

    bool auth(sock_t fd);

  private:
    tcp::Server _tcp_serv;
    int _conn_num;
    fastring _passwd;
    std::unique_ptr<Service> _service;
    co::Pool _buffer;
};

Server::Server() {
    _p = new ServerImpl;
}

Server::~Server() {
    delete (ServerImpl*)_p;
}

void Server::add_service(Service* s) {
    ((ServerImpl*)_p)->add_service(s);
}

void Server::start(const char* ip, int port, const char* passwd) {
    ((ServerImpl*)_p)->start(ip, port, passwd);
}

void ServerImpl::on_connection(sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    
    if (!_passwd.empty() && !this->auth(fd)) {
        ELOG << "auth failed, reset connection " << fd << " 3 seconds later..";
        co::reset_tcp_socket(fd, 3000);
        return;
    }

    atomic_inc(&_conn_num);

    int r = 0, len = 0;
    Header header;
    fastring* buf = 0;
    Json req, res;

    while (true) {
        // recv req from the client
        do {
          recv_beg:
            r = co::recvn(fd, &header, sizeof(header), FLG_rpc_conn_idle_sec * 1000);

            if (unlikely(r == 0)) goto recv_zero_err;
            if (unlikely(r == -1)) {
                if (co::error() != ETIMEDOUT) goto recv_err;
                if (_conn_num > FLG_rpc_max_idle_conn) goto idle_err;

                if (buf) { 
                    buf->clear();
                    _buffer.push(buf);
                    buf = 0;
                }
                goto recv_beg;
            }

            if (unlikely(header.magic != kMagic)) goto magic_err;

            len = ntoh32(header.len);
            if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

            if (buf == NULL) buf = (fastring*) _buffer.pop();
            buf->resize(len);
            r = co::recvn(fd, (char*)buf->data(), len, FLG_rpc_recv_timeout);
            if (unlikely(r == 0)) goto recv_zero_err;
            if (unlikely(r == -1)) goto recv_err;

            req = json::parse(buf->data(), buf->size());
            if (req.is_null()) goto json_parse_err;

            RPCLOG << "rpc recv req: " << req;
        } while (0);

        // call rpc and send response to the client
        do {
            res = Json();
            _service->process(req, res);

            buf->resize(sizeof(Header));
            res.str(*(fastream*)buf);
            set_header((void*)buf->data(), (int) buf->size() - sizeof(Header));
            
            r = co::send(fd, buf->data(), (int) buf->size(), FLG_rpc_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            RPCLOG << "rpc send res: " << res;;
        } while (0);
    }

  recv_zero_err:
    LOG << "rpc client close the connection, connfd: " << fd;
    co::close(fd);
    goto cleanup;
  idle_err:
    ELOG << "rpc close idle connection, connfd: " << fd;
    co::close(fd);
    goto cleanup;
  magic_err:
    ELOG << "rpc recv error: bad magic number";
    goto err_end;
  msg_too_long_err:
    ELOG << "rpc recv error: body too long: " << len;
    goto err_end;
  recv_err:
    ELOG << "rpc recv error: " << co::strerror();
    goto err_end;
  send_err:
    ELOG << "rpc send error: " << co::strerror();
    goto err_end;
  json_parse_err:
    ELOG << "rpc json parse error: " << *buf;
    goto err_end;
  err_end:
    co::reset_tcp_socket(fd, 1000);
  cleanup:
    atomic_dec(&_conn_num);
    if (buf) {
        buf->clear();
        _buffer.push(buf);
    }
}

bool ServerImpl::auth(sock_t fd) {
    static const fastring kAuth("auth");

    int r = 0, len = 0;
    Header header;
    fastream fs;
    Json req, res;
    char buf[sizeof(json::Value)];
    json::Value& x = *(json::Value*)buf;

    // wait for the first req from client, timeout in 7 seconds
    do {
        r = co::recvn(fd, &header, sizeof(header), 7000);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (header.magic != kMagic) goto magic_err;

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        req = json::parse(fs.data(), fs.size());
        if (req.is_null()) goto json_parse_err;

        x = req["method"];
        if (!x.is_string() || x.get_string() != kAuth) {
            ELOG << "auth method not found in the req";
            return false;
        }
    } while (0);

    // send auth require to the client
    do {
        res.add_member("method", "auth");
        res.add_member("nonce", str::from(now::us()));
        res.add_member("err", 401);
        res.add_member("errmsg", "401 Unauthorized");

        fs.resize(sizeof(Header));
        res.str(fs);
        set_header((void*)fs.data(), (int)fs.size() - sizeof(Header));

        r = co::send(fd, fs.data(), (int)fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        DLOG << "send auth require to the client: " << (fs.data() + sizeof(Header));
    } while (0);

    // wait for the auth answer from the client
    do {
        r = co::recvn(fd, &header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (header.magic != kMagic) goto magic_err;

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        req = json::parse(fs.data(), fs.size());
        if (req.is_null()) goto json_parse_err;

        DLOG << "recv auth response from the client: " << fs;

        x = req["method"];
        if (!x.is_string() || x.get_string() != kAuth) {
            ELOG << "auth method not found in the req";
            return false;
        }
    } while (0);

    // send the final response to the client
    do {
        x = req["md5"];
        if (!x.is_string() || x.get_string() != md5sum(_passwd + res["nonce"].get_string())) {
            res = Json();
            res.add_member("method", "auth");
            res.add_member("err", 401);
            res.add_member("errmsg", "401 auth failed");

            fs.resize(sizeof(Header));
            res.str(fs);
            set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));

            r = co::send(fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            DLOG << "send auth result to client: " << (fs.c_str() + sizeof(Header));
            return false;
        } else {
            res = Json();
            res.add_member("method", "auth");
            res.add_member("err", 200);
            res.add_member("errmsg", "200 auth ok");

            fs.resize(sizeof(Header));
            res.str(fs);
            set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));

            r = co::send(fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            DLOG << "send auth result to client: " << (fs.c_str() + sizeof(Header));
            return true;
        }
    } while (0);

  magic_err:
    ELOG << "recv error: bad magic number";
    return false;
  msg_too_long_err:
    ELOG << "recv error: body too long: " << len;
    return false;
  recv_zero_err:
    LOG << "client close the connection, fd: " << fd;
    return false;
  recv_err:
    ELOG << "recv error: " << co::strerror();
    return false;
  send_err:
    ELOG << "send error: " << co::strerror();
    return false;
  json_parse_err:
    ELOG << "json parse error: " << fs;
    return false;
}

class ClientImpl {
  public:
    ClientImpl(const char* ip, int port, const char* passwd)
        : _tcp_cli(ip, port) {
        if (passwd && *passwd) _passwd = md5sum(passwd);
    }

    ~ClientImpl() = default;

    void call(const Json& req, Json& res);

  private:
    tcp::Client _tcp_cli;
    fastring _passwd;
    fastream _fs;

    bool auth();
    bool connect();
};

Client::Client(const char* ip, int port, const char* passwd) {
    _p = new ClientImpl(ip, port, passwd);
}

Client::~Client() {
    delete (ClientImpl*)_p;
}

void Client::call(const Json& req, Json& res) {
    return ((ClientImpl*)_p)->call(req, res);
}

bool ClientImpl::connect() {
    if (!_tcp_cli.connect(FLG_rpc_conn_timeout)) return false;
    if (!_passwd.empty() && !this->auth()) {
        _tcp_cli.disconnect();
        return false;
    }
    return true;
}

void ClientImpl::call(const Json& req, Json& res) {
    int r = 0, len = 0;
    Header header;

    if (!_tcp_cli.connected() && !this->connect()) return;

    // send request
    do {
        _fs.resize(sizeof(Header));
        req.str(_fs);
        set_header((void*)_fs.data(), (int)_fs.size() - sizeof(Header));

        r = _tcp_cli.send(_fs.data(), (int)_fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        RPCLOG << "rpc send req: " << req;
    } while (0);

    // wait for response
    do {
        r = _tcp_cli.recvn(&header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (unlikely(header.magic != kMagic)) goto magic_err;

        len = ntoh32(header.len);
        if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

        _fs.resize(len);
        r = _tcp_cli.recvn((char*) _fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        res = json::parse(_fs.c_str(), _fs.size());
        if (res.is_null()) goto json_parse_err;
        RPCLOG << "rpc recv res: " << res;
        return;
    } while (0);

  magic_err:
    ELOG << "rpc recv error: bad magic number";
    goto err_end;
  msg_too_long_err:
    ELOG << "rpc recv error: body too long: " << len;
    goto err_end;
  recv_zero_err:
    ELOG << "rpc server close the connection..";
    goto err_end;
  recv_err:
    ELOG << "rpc recv error: " << co::strerror();
    goto err_end;
  send_err:
    ELOG << "rpc send error: " << co::strerror();
    goto err_end;
  json_parse_err:
    ELOG << "rpc json parse error: " << _fs;
    goto err_end;
  err_end:
    _tcp_cli.disconnect();
}

bool ClientImpl::auth() {
    int r = 0, len = 0;
    Header header;
    fastream fs;
    Json req, res;
    char buf[sizeof(json::Value)];
    json::Value& x = *(json::Value*)buf;

    // send the first auth req
    do {
        req.add_member("method", "auth");
        fs.resize(sizeof(header));
        req.str(fs);
        set_header((void*)fs.data(), (int) fs.size() - sizeof(header));

        r = _tcp_cli.send(fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;
    } while (0);

    // recv the first response from server
    do {
        r = _tcp_cli.recv(&header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (header.magic != kMagic) goto magic_err;        

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = _tcp_cli.recvn((char*)fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        res = json::parse(fs.data(), fs.size());
        if (res.is_null()) goto json_parse_err;

        DLOG << "recv auth request from server: " << fs;
    } while (0);

    // get nonce and calculate the required md5, and send it to the server
    do {
        x = res["nonce"];
        if (unlikely(!x.is_string())) {
            ELOG << "nonce not found..";
            return false;
        }

        req.add_member("md5", md5sum(_passwd + x.get_string()));

        fs.resize(sizeof(header));
        req.str(fs);
        set_header((void*)fs.data(), (int) fs.size() - sizeof(header));

        r = _tcp_cli.send(fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        DLOG << "send auth answer to the server: " << (fs.c_str() + sizeof(header));
    } while (0);

    // recv the final auth response from server
    do {
        r = _tcp_cli.recv(&header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (header.magic != kMagic) goto magic_err;        

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = _tcp_cli.recvn((char*)fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        res = json::parse(fs.data(), fs.size());
        if (res.is_null()) goto json_parse_err;

        DLOG << "recv auth result from the server: " << fs;

        x = res["err"];
        if (!x.is_int() || x.get_int() != 200) {
            ELOG << "auth failed..";
            return false;
        }

        return true;
    } while (0);

  magic_err:
    ELOG << "recv error: bad magic number";
    return false;
  msg_too_long_err:
    ELOG << "recv error: body too long: " << len;
    return false;
  recv_zero_err:
    ELOG << "server close the connection..";
    return false;
  recv_err:
    ELOG << "recv error: " << co::strerror();
    return false;
  send_err:
    ELOG << "send error: " << co::strerror();
    return false;
  json_parse_err:
    ELOG << "json parse error: " << fs;
    return false;
}

} // rpc
