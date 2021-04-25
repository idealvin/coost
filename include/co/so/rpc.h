#pragma once

#include "tcp.h"
#include "../json.h"

namespace so {
namespace rpc {

class Service {
  public:
    Service() = default;
    virtual ~Service() = default;

    virtual void process(const Json& req, Json& res) = 0;
};

class Server {
  public:
    Server();
    ~Server();

    void add_service(Service* s);
    void start(const char* ip, int port, const char* passwd="");

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

class Client {
  public:
    Client(const char* ip, int port, const char* passwd);
    ~Client() = default;

    void call(const Json& req, Json& res);

    void ping() {
        Json req, res;
        req.add_member("method", "ping");
        this->call(req, res);
    }

  private:
    tcp::Client _tcp_cli;
    fastring _passwd;
    fastream _fs;

    bool auth();
    bool connect();
    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // rpc
} // so

namespace rpc = so::rpc;
