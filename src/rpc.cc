#include "co/rpc.h"
#include "co/flag.h"
#include "co/log.h"
#include "co/co.h"

#define SS(name, c, e) static const char* name[2] = { c, e };
SS(s_rpc_max_msg_size, "@i RPC 最大消息长度", "@i max size of RPC message");
SS(s_rpc_recv_timeout, "@i RPC 接收超时时间(毫秒)", "@i RPC recv timeout(ms)");
SS(s_rpc_send_timeout, "@i RPC 发送超时时间(毫秒)", "@i RPC send timeout(ms)");
SS(s_rpc_conn_timeout, "@i RPC 连接超时时间(毫秒)", "@i RPC connect timeout(ms)");
SS(s_rpc_conn_idle_sec, "@i RPC 连接最大空闲时间(秒)", "@i RPC max connection idle time(seconds)");
SS(s_rpc_max_idle_conn, "@i RPC 最大空闲连接数", "@i max idle RPC connection");
SS(s_rpc_log, "@i 打印RPC日志", "@i print RPC log");

DEF_int32(rpc_max_msg_size, 8 << 20, s_rpc_max_msg_size);
DEF_int32(rpc_recv_timeout, 3000, s_rpc_recv_timeout);
DEF_int32(rpc_send_timeout, 3000, s_rpc_send_timeout);
DEF_int32(rpc_conn_timeout, 3000, s_rpc_conn_timeout);
DEF_int32(rpc_conn_idle_sec, 180, s_rpc_conn_idle_sec);
DEF_int32(rpc_max_idle_conn, 128, s_rpc_max_idle_conn);
DEF_bool(rpc_log, false, s_rpc_log);

#define RPCLOG if (FLG_rpc_log) log::info

namespace co {

struct Header {
    uint16 flags; // reserved, 0
    uint16 magic; // 0x7777
    uint32 len;   // body len
}; // 8 bytes

static const uint16 g_magic = 0x7777;

inline void set_header(void* header, uint32 msg_len) {
    ((Header*)header)->flags = 0;
    ((Header*)header)->magic = g_magic;
    ((Header*)header)->len = co::hton32(msg_len);
}

struct rpc_server_impl {
    static void ping(json::any&, json::any& res) {
        res.add_member("res", "pong");
    }

    rpc_server_impl(const char* ip, int port)
        : _tcp_serv(ip, (uint16)port) {
        _methods["ping"] = &rpc_server_impl::ping;
    }

    ~rpc_server_impl() = default;

    void add_service(co::unique<rpc_service>&& s);

    rpc_method_t* find_method(const char* name);

    void on_connection(sock_t fd);

    void start();
    void stop() { _tcp_serv.stop(); }
    void process(json::any& req, json::any& res);

    co::tcp_server _tcp_serv;
    co::hash_map<const char*, co::unique<rpc_service>> _services;
    co::hash_map<const char*, rpc_method_t> _methods;
};

void rpc_server_impl::add_service(co::unique<rpc_service>&& s) {
    const char* const service_name = s->name();
    const auto& methods = s->methods();
    auto r = _services.emplace(service_name, std::move(s));
    log::check(r.second, "service already added: ", service_name);

    for (auto& method : methods) {
        auto r = _methods.emplace(method.first, method.second);
        log::check(r.second, "method already added: ", method.first);
    }
}

inline rpc_method_t* rpc_server_impl::find_method(const char* name) {
    auto it = _methods.find(name);
    return it != _methods.end() ? &it->second : nullptr;
}

void rpc_server_impl::start() {
    _tcp_serv.on_connection([this](sock_t fd) {
        this->on_connection(fd);
    }).start();
}

void rpc_server_impl::process(json::any& req, json::any& res) {
    auto& x = req.get("api");
    if (x.is_string()) {
        auto m = this->find_method(x.as_c_str());
        if (m) {
            (*m)(req, res);
        } else {
            res.add_member("error", "api not found");
        }
    } else {
        res.add_member("error", "string filed 'api' not found in req");
    }
}

void rpc_server_impl::on_connection(sock_t fd) {
    int r = 0;
    uint32 len = 0;
    Header header;
    co::string buf;
    json::any req, res;

_beg:
    r = co::recvn(fd, &header, sizeof(header), FLG_rpc_conn_idle_sec * 1000);
    if (__unlikely(r == 0)) goto recv_zero_err;
    if (__unlikely(r < 0)) {
        if (!co::timeout()) goto recv_err;
        if (_tcp_serv.conn_num() > (uint32)FLG_rpc_max_idle_conn) goto idle_err;
        buf.reset();
        goto _beg;
    }

    if (__unlikely(header.magic != g_magic)) goto magic_err;

    len = co::ntoh32(header.len);
    if (__unlikely(len > (uint32)FLG_rpc_max_msg_size)) goto msg_too_long_err;

    buf.resize(len);
    r = co::recvn(fd, buf.data(), len, FLG_rpc_recv_timeout);
    if (__unlikely(r == 0)) goto recv_zero_err;
    if (__unlikely(r < 0)) goto recv_err;

    req = json::parse(buf.data(), buf.size());
    if (__unlikely(req.is_null())) goto json_parse_err;

    RPCLOG("rpc recv req: ", buf);
    res.reset();
    this->process(req, res);

    buf.resize(sizeof(Header));
    buf << res;
    set_header(buf.data(), (uint32)(buf.size() - sizeof(Header)));
    
    r = co::send(fd, buf.data(), (int)buf.size(), FLG_rpc_send_timeout);
    if (__unlikely(r <= 0)) goto send_err;

    RPCLOG(
        "rpc send res: ",
        std::string_view(buf.data() + sizeof(Header), buf.size() - sizeof(Header))
    );
    goto _beg;

recv_zero_err:
    log::info("rpc client close the connection, connfd: ", fd);
    co::close(fd);
    goto end;
idle_err:
    log::info("rpc close idle connection, connfd: ", fd);
    co::reset_tcp_socket(fd);
    goto end;
magic_err:
    log::error("rpc recv error: bad magic number");
    goto reset_conn;
msg_too_long_err:
    log::error("rpc recv error: body too long: ", len);
    goto reset_conn;
recv_err:
    log::error("rpc recv error: ", co::strerror());
    goto reset_conn;
send_err:
    log::error("rpc send error: ", co::strerror());
    goto reset_conn;
json_parse_err:
    log::error("rpc json parse error: ", buf);
    goto reset_conn;
reset_conn:
    co::reset_tcp_socket(fd, 1000);
end:
    return;
}

rpc_server::rpc_server(const char* ip, int port) {
    _p = co::_new<rpc_server_impl>(ip, port);
    runtime_assert(_p);
}

rpc_server::~rpc_server() {
    if (_p) {
        co::_delete((rpc_server_impl*)_p);
        _p = 0;
    }
}

rpc_server& rpc_server::add_service(co::unique<rpc_service>&& s) {
    static_cast<rpc_server_impl*>(_p)->add_service(std::move(s));
    return *this;
}

void rpc_server::start() { static_cast<rpc_server_impl*>(_p)->start(); }

void rpc_server::stop() { static_cast<rpc_server_impl*>(_p)->stop(); }


void rpc_client::call(const json::any& req, json::any& res) {
    int r = 0;
    uint32 len = 0;
    Header header;
    co::string buf;
    if (!_tcp_cli.connected() && !_tcp_cli.connect(FLG_rpc_conn_timeout)) return;

    buf.reserve(1024);
    buf.resize(sizeof(Header));
    buf << req;
    set_header(buf.data(), (uint32)(buf.size() - sizeof(Header)));

    r = _tcp_cli.send(buf.data(), (int)buf.size(), FLG_rpc_send_timeout);
    if (__unlikely(r <= 0)) goto send_err;
    RPCLOG(
        "rpc send req: ",
        std::string_view(buf.data() + sizeof(Header), buf.size() - sizeof(Header)) 
    );

    r = _tcp_cli.recvn(&header, sizeof(header), FLG_rpc_recv_timeout);
    if (__unlikely(r == 0)) goto recv_zero_err;
    if (__unlikely(r < 0)) goto recv_err;
    if (__unlikely(header.magic != g_magic)) goto magic_err;

    len = co::ntoh32(header.len);
    if (__unlikely(len > (uint32)FLG_rpc_max_msg_size)) goto msg_too_long_err;

    buf.resize(len);
    r = _tcp_cli.recvn(buf.data(), len, FLG_rpc_recv_timeout);
    if (__unlikely(r == 0)) goto recv_zero_err;
    if (__unlikely(r < 0)) goto recv_err;

    res = json::parse(buf.data(), buf.size());
    if (res.is_null()) goto json_parse_err;
    RPCLOG("rpc recv res: ", buf);
    return;

magic_err:
    log::error("rpc recv error: bad magic number");
    goto end;
msg_too_long_err:
    log::error("rpc recv error: body too long");
    goto end;
recv_zero_err:
    log::error("rpc server reset the connection..");
    goto end;
recv_err:
    log::error("rpc recv error: ", co::strerror());
    goto end;
send_err:
    log::error("rpc send error: ", co::strerror());
    goto end;
json_parse_err:
    log::error("rpc json parse error: ", buf);
    goto end;
end:
    _tcp_cli.disconnect();
}

void rpc_client::ping() {
    json::any req({{"api", "ping"}}), res;
    this->call(req, res);
}

namespace xx {

static int g_nifty_counter;

RpcInit::RpcInit() {
    const int n = ++g_nifty_counter;
    if (n == 2) {
        flag::run_before_parse([]() {
            flag::unhide("rpc_max_msg_size");
            flag::unhide("rpc_recv_timeout");
            flag::unhide("rpc_send_timeout");
            flag::unhide("rpc_conn_timeout");
            flag::unhide("rpc_conn_idle_sec");
            flag::unhide("rpc_max_idle_conn");
            flag::unhide("rpc_log");
        });
    }
}

} // xx
} // co
