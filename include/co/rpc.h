#pragma once

#include "json.h"
#include "stl.h"
#include "tcp.h"
#include <functional>

namespace co {
namespace xx {

struct RpcInit {
    RpcInit();
    ~RpcInit() = default;
};

static RpcInit g_rpc_init;

} // xx

using rpc_method_t = std::function<void(json::any&, json::any&)>;

struct rpc_service {
    rpc_service() = default;
    virtual ~rpc_service() = default;

    virtual const char* name() const = 0;
    virtual const co::map<const char*, rpc_method_t>& methods() const = 0;
};

struct rpc_server {
    rpc_server(const char* ip, int port);
    ~rpc_server();

    rpc_server(const rpc_server&) = delete;
    rpc_server(rpc_server&&) = delete;
    void operator=(const rpc_server&) = delete;
    void operator=(rpc_server&&) = delete;

    rpc_server& add_service(co::unique<rpc_service>&& s);
    void start();
    void stop();

    void* _p;
};

struct rpc_client {
    rpc_client(const char* server_host, int server_port)
        : _tcp_cli(server_host, (uint16)server_port) {
    }

    rpc_client(const rpc_client& c)
        : _tcp_cli(c._tcp_cli) {
    }

    ~rpc_client() = default;

    rpc_client(rpc_client&&) = delete;
    void operator=(const rpc_client& c) = delete;
    void operator=(rpc_client&&) = delete;

    void call(const json::any& req, json::any& res);
    void ping();
    void close() { _tcp_cli.close(); }

    co::tcp_client _tcp_cli;
};

} // co

namespace rpc {

using service = co::rpc_service;
using server = co::rpc_server;
using client = co::rpc_client;
using method_t = co::rpc_method_t;

} // rpc
