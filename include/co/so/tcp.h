#pragma once

#include "../co.h"
#include "../fastream.h"

namespace so {
namespace tcp {

struct Connection {
    sock_t fd;   // conn fd
    fastring ip; // peer ip
    int port;    // peer port
    void* p;     // pointer to Server where this connection was accepted
};

// Tcp server based on coroutine.
// Support both ipv4 and ipv6. One coroutine per connection.
class Server {
  public:
    // @ip is either a ipv4 or ipv6 address.
    Server(const char* ip, int port)
        : _ip((ip && *ip) ? ip : "0.0.0.0"), _port(port) {
    }

    virtual ~Server() = default;

    // Run the server loop in coroutine.
    virtual void start() {
        go(&Server::loop, this);
    }

    // The derived class must implement this method.
    // The @conn was created by operator new. Remember to delete it 
    // when the connection was closed.
    virtual void on_connection(Connection* conn) = 0;

  protected:
    fastring _ip;
    uint32 _port;

  private:
    // The server loop, listen on the port and wait for connections.
    // Call on_connection() in a new coroutine for every connection. 
    void loop();

    DISALLOW_COPY_AND_ASSIGN(Server);
};

// Tcp client based on coroutine.
// Support both ipv4 and ipv6. One client corresponds to one connection.
// The Client MUST be used in coroutine, and it is not coroutine-safe.
class Client {
  public:
    // @ip is a domain name, or either a ipv4 or ipv6 address.
    Client(const char* ip, int port)
        : _ip((ip && *ip) ? ip : "127.0.0.1"), _port(port),
          _fd((sock_t)-1), _sched_id(-1) {
    }

    virtual ~Client() { this->disconnect(); }

    int recv(void* buf, int n, int ms=-1) {
        return co::recv(_fd, buf, n, ms);
    }

    int recvn(void* buf, int n, int ms=-1) {
        return co::recvn(_fd, buf, n, ms);
    }

    int send(const void* buf, int n, int ms=-1) {
        return co::send(_fd, buf, n, ms);
    }

    bool connected() const { return _fd != (sock_t)-1; }

    // @ms: timeout in milliseconds
    bool connect(int ms);

    // MUST be called in the thread where it is connected.
    void disconnect() {
        if (this->connected()) {
            assert(_sched_id == -1 || _sched_id == co::sched_id());
            co::close(_fd);
            _fd = (sock_t)-1;
            _sched_id = -1;
        }
    }

  protected:
    fastring _ip;
    uint32 _port;
    int _sched_id;  // id of scheduler where this client runs in
    sock_t _fd;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // tcp

using tcp::Connection;
} // so

namespace tcp = so::tcp;

inline fastream& operator<<(fastream& fs, const so::Connection& c) {
    return fs << c.ip << ':' << c.port;
}
