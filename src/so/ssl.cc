#ifdef CO_SSL

#include "co/so/ssl.h"
#include "co/co.h"
#include "co/log.h"
#include "co/fastream.h"
#include "co/thread.h"


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
            //DLOG << "SSL_accept return 0, error: " << SSL_get_error(s, 0);
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
            //DLOG << "SSL_accept return " << r << ", error: " << e;
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
            //DLOG << "SSL_connect return 0, error: " << SSL_get_error(s, 0);
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
            //DLOG << "SSL_connect return " << r << ", error: " << e;
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
            //DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
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
            //DLOG << "SSL_read return " << r << ", error: " << e;
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
            //DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
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
                //DLOG << "SSL_read return " << r << ", error: " << e;
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
            //DLOG << "SSL_write return 0, error: " << SSL_get_error(s, 0);
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
                //DLOG << "SSL_write return " << r << ", error: " << e;
                return r;
            }
        } else {
            remain -= r;
            p += r;
        }
    } while (true);
}

bool timeout() { return co::timeout(); }

} // ssl

#endif
