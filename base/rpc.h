#pragma once

#include "json.h"

namespace rpc {

class Service {
  public:
    Service() = default;
    virtual ~Service() = default;

    virtual void process(const Json& req, Json& res) = 0;
};

class Server {
  public:
    Server() = default;
    virtual ~Server() = default;

    virtual void start() = 0;
    virtual void add_service(Service*) = 0;
};

class Client {
  public:
    Client() = default;
    virtual ~Client() = default;

    virtual void call(const Json& req, Json& res) = 0;
};

Server* new_server(const char* ip, int port, const char* passwd);
Client* new_client(const char* ip, int port, const char* passwd);

} // rpc
