#include "rpc.h"
#include "co.h"
#include "flag.h"
#include "log.h"
#include "hash.h"
#include "fastring.h"
#include "fastream.h"
#include "str.h"
#include "time.h"

#include <memory>

DEF_int32(rpc_max_msg_size, 8 << 20, "#2 max size of rpc message, default: 8M");
DEF_int32(rpc_recv_timeout, 1024, "#2 recv timeout in ms");
DEF_int32(rpc_send_timeout, 1024, "#2 send timeout in ms");
DEF_int32(rpc_conn_timeout, 3000, "#2 connect timeout in ms");
DEF_int32(rpc_conn_idle_sec, 180, "#2 connection may be closed if no data was recieved for n seconds");
DEF_int32(rpc_max_idle_conn, 1024, "#2 max idle connections");
DEF_bool(rpc_tcp_nodelay, true, "#2 enable tcp nodelay if true");
DEF_bool(rpc_log, true, "#2 enable rpc log if true");

#define RPCLOG LOG_IF(FLG_rpc_log)

namespace rpc {

struct Header {
    int32 magic; // 0xbaddad
    int32 len;   // body len
}; // 8 bytes

inline void set_header(void* header, int msg_len) {
    static const int32 kMagic = hton32(0xbaddad);
    ((Header*) header)->magic = kMagic;
    ((Header*) header)->len = hton32(msg_len);
}

struct Connection {
    sock_t fd;   // conn fd
    fastring ip; // peer ip
    int port;    // peer port
    void* p;     // ServerImpl*
};

inline fastream& operator<<(fastream& fs, const Connection& c) {
    return fs << c.ip << ':' << c.port;
}

class ServerImpl : public Server {
  public:
    ServerImpl(const char* ip, int port, const char* passwd)
        : _ip(ip), _port(port), _conn_num(0) {
        if (!_ip || !*_ip) _ip = "0.0.0.0";
        if (passwd && *passwd) _passwd = md5sum(passwd);
    }

    virtual ~ServerImpl() {
    }

    virtual void start() {
        LOG << "rpc server start, ip: " << _ip << ", port: " << _port
            << ", has password : " << !_passwd.empty();
        co::go(&ServerImpl::loop, this);
    }

    virtual void add_service(Service* service) {
        _service.reset(service);
    }

    void loop();

    void on_connection(Connection* conn);
    bool auth(Connection* conn);

  private:
    const char* _ip;
    int _port;
    int _conn_num;
    fastring _passwd;
    std::unique_ptr<Service> _service;
};

void on_new_connection(void* p) {
    Connection* c = (Connection*) p;
    ServerImpl* s = (ServerImpl*) c->p;
    s->on_connection(c);
}

void ServerImpl::loop() {
    sock_t fd = co::tcp_socket();
    co::set_reuseaddr(fd);

    sock_t connfd;
    int addrlen = sizeof(sockaddr_in);
    struct sockaddr_in addr;

    co::init_ip_addr(&addr, _ip, _port);
    const int r = co::bind(fd, &addr, sizeof(addr));
    CHECK_EQ(r, 0) << "bind (" << _ip << ':' << _port << ") failed: " << co::strerror();
    CHECK_EQ(co::listen(fd, 1024), 0) << "listen error: " << co::strerror();

    while (true) {
        connfd = co::accept(fd, &addr, &addrlen);
        if (unlikely(connfd == -1)) {
            WLOG << "accept error: " << co::strerror();
            continue;
        }

        if (unlikely(addrlen != sizeof(sockaddr_in))) {
            WLOG << "rpc server accept a connection with unexpected addrlen: " << addrlen
                 << ", addr will not be filled..";
            memset(&addr, 0, sizeof(addr));
        }

        Connection* conn = new Connection;
        conn->fd = connfd;
        conn->ip = co::ip_str(&addr);
        conn->port = addr.sin_port;
        conn->p = this;

        co::go(on_new_connection, conn);
    }
}

void ServerImpl::on_connection(Connection* conn) {
    std::unique_ptr<Connection> c(conn);
    sock_t fd = conn->fd;
    co::set_tcp_keepalive(fd);
    if (FLG_rpc_tcp_nodelay) co::set_tcp_nodelay(fd);
    
    if (!_passwd.empty() && !this->auth(conn)) {
        ELOG << "auth failed, reset connection from " << *conn << " 3 seconds later..";
        co::reset_tcp_socket(fd, 3000);
        return;
    }

    LOG << "rpc server accept new connection: " << *conn << ", conn fd: " << fd
        << ", conn num: " << atomic_inc(&_conn_num); // << " now: " << now::us();

    int r = 0, len = 0;
    Header header;
    fastream fs;
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
                if (fs.capacity() > 0) fs.swap(fastream()); // free the memory
                goto recv_beg;
            }

            if (unlikely(ntoh32(header.magic) != 0xbaddad)) goto magic_err;

            len = ntoh32(header.len);
            if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

            fs.resize(len);
            r = co::recvn(fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
            if (unlikely(r == 0)) goto recv_zero_err;
            if (unlikely(r == -1)) goto recv_err;

            req = json::parse(fs.data(), fs.size());
            if (req.is_null()) goto json_parse_err;

            RPCLOG << "recv req: " << req;
        } while (0);

        // call rpc and send response to the client
        do {
            res.reset();
            _service->process(req, res);

            fs.resize(sizeof(Header));
            res.str(fs);
            set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));
            
            r = co::send(fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            RPCLOG << "send res: " << res;;
        } while (0);
    }

  recv_zero_err:
    LOG << "client close the connection: " << *c; // << " now: " << now::us();
    co::close(fd);
    atomic_dec(&_conn_num);
    return ;
  idle_err:
    ELOG << "close idle connection: " << *c;
    co::reset_tcp_socket(fd);
    atomic_dec(&_conn_num);
    return ;
  magic_err:
    ELOG << "recv error: bad magic number";
    goto err_end;
  msg_too_long_err:
    ELOG << "recv error: body too long: " << len;
    goto err_end;
  recv_err:
    ELOG << "recv error: " << co::strerror();
    goto err_end;
  send_err:
    ELOG << "send error: " << co::strerror();
    goto err_end;
  json_parse_err:
    ELOG << "json parse error: " << fs;
    goto err_end;
  err_end:
    co::reset_tcp_socket(fd, 3000);
    atomic_dec(&_conn_num);
}

bool ServerImpl::auth(Connection* conn) {
    static const fastring kAuth("auth");

    int r = 0, len = 0;
    Header header;
    fastream fs;
    Json req, res, x;

    // wait for the first req from client, timeout in 7 seconds
    do {
        r = co::recvn(conn->fd, &header, sizeof(header), 7000);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (ntoh32(header.magic) != 0xbaddad) goto magic_err;

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(conn->fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
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
        set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));

        r = co::send(conn->fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        DLOG << "send auth require to the client: " << (fs.data() + sizeof(Header));
    } while (0);

    // wait for the auth answer from the client
    do {
        r = co::recvn(conn->fd, &header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (ntoh32(header.magic) != 0xbaddad) goto magic_err;

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(conn->fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
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
            res.reset();
            res.add_member("method", "auth");
            res.add_member("err", 401);
            res.add_member("errmsg", "401 auth failed");

            fs.resize(sizeof(Header));
            res.str(fs);
            set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));

            r = co::send(conn->fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
            if (unlikely(r == -1)) goto send_err;

            DLOG << "send auth result to client: " << (fs.c_str() + sizeof(Header));
            return false;
        } else {
            res.reset();
            res.add_member("method", "auth");
            res.add_member("err", 200);
            res.add_member("errmsg", "200 auth ok");

            fs.resize(sizeof(Header));
            res.str(fs);
            set_header((void*)fs.data(), (int) fs.size() - sizeof(Header));

            r = co::send(conn->fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
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
    LOG << "client close the connection: " << *conn;
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


class ClientImpl : public Client {
  public:
    ClientImpl(const char* serv_ip, int serv_port, const char* passwd);
    virtual ~ClientImpl();

    bool connect();
    void disconnect();
    virtual void ping();
    virtual void call(const Json& req, Json& res);

  private:
    const char* _serv_ip;
    int _serv_port;
    sock_t _fd;
    fastring _passwd;
    fastream _fs;

    bool auth();
};

ClientImpl::ClientImpl(const char* serv_ip, int serv_port, const char* passwd)
    : _serv_ip(serv_ip), _serv_port(serv_port), _fd(-1) {
    if (!_serv_ip || !*_serv_ip) _serv_ip = "127.0.0.1";
    if (passwd && *passwd) _passwd = md5sum(passwd);
}

ClientImpl::~ClientImpl() {
    if (co::sched_id() != -1) this->disconnect();
}

void ClientImpl::disconnect() {
    if (_fd != -1) {
        co::close(_fd);
        _fd = -1;
    }
}

bool ClientImpl::connect() {
    if (_fd != -1) return true;

    _fd = co::tcp_socket();
    if (_fd == -1) {
        ELOG << "create tcp socket failed..";
        return false;
    }

    struct sockaddr_in addr;
    co::init_ip_addr(&addr, _serv_ip, _serv_port);

    int r = co::connect(_fd, &addr, sizeof(addr), FLG_rpc_conn_timeout);
    if (r == -1) {
        ELOG << "connect to server " << _serv_ip << ':' << _serv_port
             << " failed, error: " << co::strerror();
        this->disconnect();
        return false;
    }

    if (FLG_rpc_tcp_nodelay) co::set_tcp_nodelay(_fd);

    if (!_passwd.empty() && !this->auth()) {
        this->disconnect();
        return false;
    }

    LOG << "connect to server " << _serv_ip << ':' << _serv_port << " success";
    return true;
}

void ClientImpl::ping() {
    Json req, res;
    req.add_member("method", "ping");
    this->call(req, res);
}

void ClientImpl::call(const Json& req, Json& res) {
    int r = 0, len = 0;
    Header header;

    if (_fd == -1 && !this->connect()) return;

    // send request
    do {
        _fs.resize(sizeof(Header));
        req.str(_fs);
        set_header((void*)_fs.data(), (int) _fs.size() - sizeof(Header));

        r = co::send(_fd, _fs.data(), (int) _fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        RPCLOG << "send req: " << req;
    } while (0);

    // wait for response
    do {
        r = co::recvn(_fd, &header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (unlikely(ntoh32(header.magic) != 0xbaddad)) goto magic_err;

        len = ntoh32(header.len);
        if (unlikely(len > FLG_rpc_max_msg_size)) goto msg_too_long_err;

        _fs.resize(len);
        r = co::recvn(_fd, (char*) _fs.data(), len, FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;

        res = json::parse(_fs.c_str(), _fs.size());
        if (res.is_null()) goto json_parse_err;
        RPCLOG << "recv res: " << res;
        return;
    } while (0);

  magic_err:
    ELOG << "recv error: bad magic number";
    this->disconnect();
    return;
  msg_too_long_err:
    ELOG << "recv error: body too long: " << len;
    this->disconnect();
    return;
  recv_zero_err:
    ELOG << "server close the connection..";
    this->disconnect();
    return;
  recv_err:
    ELOG << "recv error: " << co::strerror();
    this->disconnect();
    return;
  send_err:
    ELOG << "send error: " << co::strerror();
    this->disconnect();
    return;
  json_parse_err:
    ELOG << "json parse error: " << _fs;
    this->disconnect();
    return;
}

bool ClientImpl::auth() {
    int r = 0, len = 0;
    Header header;
    fastream fs;
    Json req, res, x;

    // send the first auth req
    do {
        req.add_member("method", "auth");
        fs.resize(sizeof(header));
        req.str(fs);
        set_header((void*)fs.data(), (int) fs.size() - sizeof(header));

        r = co::send(_fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;
    } while (0);

    // recv the first response from server
    do {
        r = co::recv(_fd, &header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (ntoh32(header.magic) != 0xbaddad) goto magic_err;        

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(_fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
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

        r = co::send(_fd, fs.data(), (int) fs.size(), FLG_rpc_send_timeout);
        if (unlikely(r == -1)) goto send_err;

        DLOG << "send auth answer to the server: " << (fs.c_str() + sizeof(header));
    } while (0);

    // recv the final auth response from server
    do {
        r = co::recv(_fd, &header, sizeof(header), FLG_rpc_recv_timeout);
        if (unlikely(r == 0)) goto recv_zero_err;
        if (unlikely(r == -1)) goto recv_err;
        if (ntoh32(header.magic) != 0xbaddad) goto magic_err;        

        len = ntoh32(header.len);
        if (len > FLG_rpc_max_msg_size) goto msg_too_long_err;

        fs.resize(len);
        r = co::recvn(_fd, (char*)fs.data(), len, FLG_rpc_recv_timeout);
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

Server* new_server(const char* ip, int port, const char* passwd) {
    return new ServerImpl(ip, port, passwd);
}

Client* new_client(const char* ip, int port, const char* passwd) {
    return new ClientImpl(ip, port, passwd);
}

} // rpc
