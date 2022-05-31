#ifdef HAS_OPENSSL
#include "co/ssl.h"
#include "co/co.h"
#include "co/log.h"
#include "co/fastream.h"
#include "co/thread.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace ssl {

static int errcb(const char* p, size_t n, void* u) {
    fastream* s = (fastream*) u;
    s->append(p, n).append(". ");
    return 0;
}

const char* strerror(S* s) {
    static thread_ptr<fastream> fs;
    if (fs == NULL) fs.reset(new fastream(256));
    fs->clear();

    if (ERR_peek_error() != 0) {
        ERR_print_errors_cb(errcb, fs.get());
    } else if (co::error() != 0) {
        fs->append(co::strerror());
    } else if (s) {
        int e = SSL_get_error((SSL*)s, 0);
        (*fs) << "ssl error: " << e;
    } else {
        fs->append("success");
    }

    return fs->c_str();
}

C* new_ctx(char c) {
    static bool x = []() {
        (void) SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        return true;
    }();
    (void)x;
    return (C*) SSL_CTX_new(c == 's' ? TLS_server_method(): TLS_client_method());
}

void free_ctx(C* c) {
    SSL_CTX_free((SSL_CTX*)c);
}

void* new_ssl(C* c) {
    return (void*) SSL_new((SSL_CTX*)c);
}

void free_ssl(S* s) {
    SSL_free((SSL*)s);
}

int set_fd(S* s, int fd) {
    return SSL_set_fd((SSL*)s, fd);
}

int get_fd(const S* s) {
    return SSL_get_fd((const SSL*)s);
}

int use_private_key_file(C* c, const char* path) {
    return SSL_CTX_use_PrivateKey_file((SSL_CTX*)c, path, SSL_FILETYPE_PEM);
}

int use_certificate_file(C* c, const char* path) {
    return SSL_CTX_use_certificate_file((SSL_CTX*)c, path, SSL_FILETYPE_PEM);
}

int check_private_key(const C* c) {
    return SSL_CTX_check_private_key((const SSL_CTX*)c);
}

int shutdown(S* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    // openssl says SSL_shutdown must not be called on error SSL_ERROR_SYSCALL 
    // and SSL_ERROR_SSL, see more details here:
    //   https://www.openssl.org/docs/man1.1.0/man3/SSL_get_error.html
    e = SSL_get_error((SSL*)s, 0);
    if (e == SSL_ERROR_SYSCALL || e == SSL_ERROR_SSL) return -1;

    do {
        ERR_clear_error();
        r = SSL_shutdown((SSL*)s);
        if (r == 1) return 1; // success
        if (r == 0) {
            DLOG << "SSL_shutdown return 0, call again..";
            continue;
        }

        e = SSL_get_error((SSL*)s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::ev_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::ev_write);
            if (!ev.wait(ms)) return -1;
        } else {
            DLOG << "SSL_shutdown return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int accept(S* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_accept((SSL*)s);
        if (r == 1) return 1; // success
        if (r == 0) {
            //DLOG << "SSL_accept return 0, error: " << SSL_get_error(s, 0);
            return 0; // ssl connection shut down
        }

        e = SSL_get_error((SSL*)s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::ev_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::ev_write);
            if (!ev.wait(ms)) return -1;
        } else {
            //DLOG << "SSL_accept return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int connect(S* s, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_connect((SSL*)s);
        if (r == 1) return 1; // success
        if (r == 0) {
            //DLOG << "SSL_connect return 0, error: " << SSL_get_error(s, 0);
            return 0; // ssl connection shut down
        }

        e = SSL_get_error((SSL*)s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::ev_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::ev_write);
            if (!ev.wait(ms)) return -1;
        } else {
            //DLOG << "SSL_connect return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int recv(S* s, void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    do {
        ERR_clear_error();
        r = SSL_read((SSL*)s, buf, n);
        if (r > 0) return r; // success
        if (r == 0) {
            //DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }
 
        e = SSL_get_error((SSL*)s, r);
        if (e == SSL_ERROR_WANT_READ) {
            co::IoEvent ev(fd, co::ev_read);
            if (!ev.wait(ms)) return -1;
        } else if (e == SSL_ERROR_WANT_WRITE) {
            co::IoEvent ev(fd, co::ev_write);
            if (!ev.wait(ms)) return -1;
        } else {
            //DLOG << "SSL_read return " << r << ", error: " << e;
            return r;
        }
    } while (true);
}

int recvn(S* s, void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    char* p = (char*) buf;
    int remain = n;

    do {
        ERR_clear_error();
        r = SSL_read((SSL*)s, p, remain);
        if (r == remain) return n; // success
        if (r == 0) {
            //DLOG << "SSL_read return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }

        if (r < 0) {
            e = SSL_get_error((SSL*)s, r);
            if (e == SSL_ERROR_WANT_READ) {
                co::IoEvent ev(fd, co::ev_read);
                if (!ev.wait(ms)) return -1;
            } else if (e == SSL_ERROR_WANT_WRITE) {
                co::IoEvent ev(fd, co::ev_write);
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

int send(S* s, const void* buf, int n, int ms) {
    CHECK(co::scheduler()) << "must be called in coroutine..";
    int r, e;
    int fd = SSL_get_fd((SSL*)s);
    if (fd < 0) return -1;

    const char* p = (const char*) buf;
    int remain = n;

    do {
        ERR_clear_error();
        r = SSL_write((SSL*)s, p, remain);
        if (r == remain) return n; // success
        if (r == 0) {
            //DLOG << "SSL_write return 0, error: " << SSL_get_error(s, 0);
            return 0;
        }

        if (r < 0) {
            e = SSL_get_error((SSL*)s, r);
            if (e == SSL_ERROR_WANT_READ) {
                co::IoEvent ev(fd, co::ev_read);
                if (!ev.wait(ms)) return -1;
            } else if (e == SSL_ERROR_WANT_WRITE) {
                co::IoEvent ev(fd, co::ev_write);
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

#else

#include "co/ssl.h"
#include "co/log.h"

namespace ssl {

const char* strerror(S*) { return 0; }

C* new_ctx(char) {
    CHECK(false)
        << "To use SSL features, please build libco with openssl 1.1.0+ as follow: \n"
        << "xmake f --with_openssl=true\n"
        << "xmake -v";
    return 0;
}

void free_ctx(C*) {}
S* new_ssl(C*) { return 0; }
void free_ssl(S*) {}
int set_fd(S*, int) { return 0; }
int get_fd(const S*) { return 0; }
int use_private_key_file(C*, const char*) { return 0; }
int use_certificate_file(C*, const char*) { return 0; }
int check_private_key(const C*) { return 0; }
int shutdown(S*, int) { return 0; }
int accept(S*, int) { return 0; }
int connect(S*, int) { return 0; }
int recv(S*, void*, int, int) { return 0; }
int recvn(S*, void*, int, int) { return 0; }
int send(S*, const void*, int, int) { return 0; }
bool timeout() { return false; }

} // ssl

#endif
