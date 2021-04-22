#ifdef CO_SSL

#include "co/so/ssl.h"
#include "co/co.h"
#include "co/log.h"
#include "co/fastream.h"
#include "co/thread.h"

DEF_int32(ssl_handshake_timeout, 3000, "#2 ssl handshake timeout in ms");

namespace so {
namespace ssl {

static int errcb(const char* p, size_t n, void* u) {
    fastream* s = (fastream*) u;
    s->append(p, n).append(". ");
    return 0;
}

const char* strerror(SSL* s) {
    static thread_ptr<fastream> fs;
    if (fs == NULL) fs.reset(new fastream(256));
    fs->clear();

    if (ERR_peek_error() != 0) {
        ERR_print_errors_cb(errcb, fs.get());
    } else if (co::error() != 0) {
        fs->append(co::strerror());
    } else if (s) {
        int e = ssl::get_error(s, 0);
        (*fs) << "ssl error: " << e;
    } else {
        fs->append("success");
    }

    return fs->c_str();
}

int shutdown(SSL* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = ssl::get_fd(s);
    if (fd < 0) return -1;

    // openssl says SSL_shutdown must not be called on error SSL_ERROR_SYSCALL 
    // and SSL_ERROR_SSL, see more details here:
    //   https://www.openssl.org/docs/man1.1.0/man3/SSL_get_error.html
    e = SSL_get_error(s, 0);
    if (e == SSL_ERROR_SYSCALL || e == SSL_ERROR_SSL) return -1;

    do {
        ERR_clear_error();
        r = SSL_shutdown(s);
        if (r == 1) return 1; // success
        if (r == 0) {
            DLOG << "SSL_shutdown return 0, call again..";
            continue;
        }

        e = SSL_get_error(s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::EV_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::EV_write);
            if (!ev.wait(ms)) return -1;
        } else {
            DLOG << "SSL_shutdown return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int accept(SSL* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = ssl::get_fd(s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_accept(s);
        if (r == 1) return 1; // success
        if (r == 0) {
            DLOG << "SSL_accept return 0, error: " << SSL_get_error(s, 0);
            return 0; // ssl connection shut down
        }

        e = SSL_get_error(s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::EV_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::EV_write);
            if (!ev.wait(ms)) return -1;
        } else {
            DLOG << "SSL_accept return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int connect(SSL* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd(s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_connect(s);
        if (r == 1) return 1; // success
        if (r == 0) {
            DLOG << "SSL_connect return 0, error: " << SSL_get_error(s, 0);
            return 0; // ssl connection shut down
        }

        e = SSL_get_error(s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::EV_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::EV_write);
            if (!ev.wait(ms)) return -1;
        } else {
            DLOG << "SSL_connect return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int recv(SSL* s, void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd(s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_read(s, buf, n);
        if (r > 0) return r; // success
        if (r == 0) {
            DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }
 
        e = SSL_get_error(s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::EV_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::EV_write);
            if (!ev.wait(ms)) return -1;
        } else {
            DLOG << "SSL_read return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int recvn(SSL* s, void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd(s);
    if (fd < 0) return -1;

    char* p = (char*) buf;
    int remain = n;

    do {
        ERR_clear_error();
        r = SSL_read(s, p, remain);
        if (r == remain) return n; // success
        if (r == 0) {
            DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }

        if (r < 0) {
            e = SSL_get_error(s, r);
            if (e == SSL_ERROR_WANT_READ) {
                co::IoEvent ev(fd, co::EV_read);
                if (!ev.wait(ms)) return -1;
            } else if (e == SSL_ERROR_WANT_WRITE) {
                co::IoEvent ev(fd, co::EV_write);
                if (!ev.wait(ms)) return -1;
            } else {
                DLOG << "SSL_read return " << r << ", error: " << e;
                return r;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

int send(SSL* s, const void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd(s);
    if (fd < 0) return -1;

    const char* p = (const char*) buf;
    int remain = n;

    do {
        ERR_clear_error();
        r = SSL_write(s, p, remain);
        if (r == remain) return n; // success
        if (r == 0) {
            DLOG << "SSL_write return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }

        if (r < 0) {
            e = SSL_get_error(s, r);
            if (e == SSL_ERROR_WANT_READ) {
                co::IoEvent ev(fd, co::EV_read);
                if (!ev.wait(ms)) return -1;
            } else if (e == SSL_ERROR_WANT_WRITE) {
                co::IoEvent ev(fd, co::EV_write);
                if (!ev.wait(ms)) return -1;
            } else {
                DLOG << "SSL_write return " << r << ", error: " << e;
                return r;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

Server::Server() : _ctx(0) {}
Server::~Server() { ssl::free_ctx(_ctx); }

void Server::start(const char* ip, int port, const char* key, const char* ca) {
    _ctx = ssl::new_server_ctx();
    CHECK(_ctx != NULL) << "ssl new server contex error: " << ssl::strerror();
    CHECK_NOTNULL(key) << "private key file must be set..";
    CHECK_NOTNULL(ca) << "certificate file must be set..";

    int r;
    r = ssl::use_private_key_file(_ctx, key);
    CHECK_EQ(r, 1) << "ssl use private key file (" << key << ") error: " << ssl::strerror();

    r = ssl::use_certificate_file(_ctx, ca);
    CHECK_EQ(r, 1) << "ssl use certificate file (" << ca << ") error: " << ssl::strerror();

    r = ssl::check_private_key(_ctx);
    CHECK_EQ(r, 1) << "ssl check private key error: " << ssl::strerror();

    _tcp_serv.on_connection(&Server::on_tcp_connection, this);
    _tcp_serv.start(ip, port);
}

void Server::on_tcp_connection(sock_t fd) {
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);

    SSL* s = ssl::new_ssl(_ctx);
    if (s == NULL) goto new_ssl_err;

    if (ssl::set_fd(s, fd) != 1) goto set_fd_err;
    if (ssl::accept(s, FLG_ssl_handshake_timeout) <= 0) goto accept_err;

    _on_ssl_connection(s);
    return;

  new_ssl_err:
    ELOG << "new SSL failed: " << ssl::strerror();
    goto err_end;
  set_fd_err:
    ELOG << "ssl set fd (" << fd << ") failed: " << ssl::strerror(s);
    goto err_end;
  accept_err:
    ELOG << "ssl accept failed: " << ssl::strerror(s);
    goto err_end;
  err_end:
    if (s) ssl::free_ssl(s);
    co::close(fd, 1000);
    return;
}

Client::Client(const char* serv_ip, int serv_port)
    : _tcp_cli(serv_ip, serv_port), _ctx(0), _ssl(0) {
}

bool Client::connect(int ms) {
    if (this->connected()) return true;
    if (!_tcp_cli.connect(ms)) return false;
    if ((_ctx = ssl::new_client_ctx()) == NULL) goto new_ctx_err;
    if ((_ssl = ssl::new_ssl(_ctx)) == NULL) goto new_ssl_err;
    if (ssl::set_fd(_ssl, _tcp_cli.fd()) != 1) goto set_fd_err;
    if (ssl::connect(_ssl, ms) != 1) goto connect_err;
    return true;
  
  new_ctx_err:
    ELOG << "ssl connect new client contex error: " << ssl::strerror();
    goto err_end;
  new_ssl_err:
    ELOG << "ssl connect new SSL failed: " << ssl::strerror();
    goto err_end;
  set_fd_err:
    ELOG << "ssl connect set fd (" << _tcp_cli.fd() << ") failed: " << ssl::strerror(_ssl);
    goto err_end;
  connect_err:
    ELOG << "ssl connect failed: " << ssl::strerror();
    goto err_end;
  err_end:
    if (_ssl) { ssl::free_ssl(_ssl); _ssl = 0; } 
    if (_ctx) { ssl::free_ctx(_ctx); _ctx = 0; }
    _tcp_cli.disconnect();
    return false;
}

/**
 * NOTE: close the underlying TCP connection directly here..
 */
void Client::disconnect() {
    if (this->connected()) {
        if (_ssl) { ssl::free_ssl(_ssl); _ssl = 0; } 
        if (_ctx) { ssl::free_ctx(_ctx); _ctx = 0; }
        _tcp_cli.disconnect();
    }
}

} // ssl
} // so

#endif
