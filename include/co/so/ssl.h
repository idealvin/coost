#pragma once

#ifdef CO_SSL
#include "tcp.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace ssl {

/**
 * get ssl error message 
 *   - openssl is good, but it's really boring to handle the errors in openssl. 
 *     See more details here: 
 *         https://en.wikibooks.org/wiki/OpenSSL/Error_handling
 * 
 *   - This function will clear the previous error message, and store the entire 
 *     openssl error queue for the current thread as a string, and the error queue 
 *     will be cleared then. 
 * 
 * @param s  a pointer to SSL, if s is not NULL, result code of the previous ssl I/O 
 *           operation will also be checked to obtain more error message.
 * 
 * @return   a pointer to the error message.
 */
const char* strerror(SSL* s=0);

/**
 * wrapper for ERR_peek_error 
 *   - Peek the earliest error code from the thread's error queue without modifying it.
 * 
 * @return  an error code or 0 if there is no error in the queue.
 */
inline unsigned long peek_error() { return ERR_peek_error(); }

/**
 * wrapper for SSL_get_error 
 *   - obtain result code for TLS/SSL I/O operation such as: 
 *     SSL_connect(), SSL_accept(), SSL_do_handshake(), SSL_read() or SSL_write()
 * 
 * @param s  a pointer to a SSL.
 * @param r  the return value of a TLS/SSL I/O function.
 * 
 * @return   a result code, see details here:
 *           https://www.openssl.org/docs/man1.1.0/man3/SSL_get_error.html
 */
inline int get_error(SSL* s, int r) { return SSL_get_error(s, r); }

/**
 * create a SSL_CTX
 * 
 * @param c  's' for server, 'c' for client.
 * 
 * @return   a pointer to SSL_CTX on success, NULL on error.
 */
inline SSL_CTX* new_ctx(char c) {
    static bool x = []() {
        (void) SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        return true;
    }();
    return SSL_CTX_new(c == 's' ? TLS_server_method(): TLS_client_method());
}

/**
 * create a SSL_CTX for server
 * 
 * @return  a pointer to SSL_CTX on success, NULL on error.
 */
inline SSL_CTX* new_server_ctx() { return new_ctx('s'); }

/**
 * create a SSL_CTX for client
 * 
 * @return  a pointer to SSL_CTX on success, NULL on error.
 */
inline SSL_CTX* new_client_ctx() { return new_ctx('c'); }

/**
 * wrapper for SSL_CTX_free
 */
inline void free_ctx(SSL_CTX* c) { SSL_CTX_free(c); }

/**
 * wrapper for SSL_new
 *   - create a SSL with the given SSL_CTX. 
 * 
 * @param c  a pointer to SSL_CTX.
 * 
 * @return   a pointer to SSL on success, NULL on error.
 */
inline SSL* new_ssl(SSL_CTX* c) { return SSL_new(c); }

/**
 * wrapper for SSL_free 
 */
inline void free_ssl(SSL* s) { SSL_free(s); }

/**
 * wrapper for SSL_set_fd 
 *   - set a socket fd to SSL. 
 * 
 * @param s   a pointer to SSL.
 * @param fd  a non-blocking socket, also overlapped on windows.
 * 
 * @return    1 on success, 0 on error.
 */
inline int set_fd(SSL* s, int fd) { return SSL_set_fd(s, fd); }

/**
 * wrapper for SSL_get_fd 
 *   - get the socket fd in a SSL. 
 * 
 * @param s  a pointer to SSL.
 * 
 * @return   a socket fd >= 0 on success, or -1 on error.
 */
inline int get_fd(const SSL* s) { return SSL_get_fd(s); }

/**
 * wrapper for SSL_CTX_use_PrivateKey_file 
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the private key.
 * 
 * @return      1 on success, otherwise failed.
 */
inline int use_private_key_file(SSL_CTX* c, const char* path) {
    return SSL_CTX_use_PrivateKey_file(c, path, SSL_FILETYPE_PEM);
}

/**
 * wrapper for SSL_CTX_use_certificate_file 
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the certificate.
 * 
 * @return      1 on success, otherwise failed.
 */
inline int use_certificate_file(SSL_CTX* c, const char* path) {
    return SSL_CTX_use_certificate_file(c, path, SSL_FILETYPE_PEM);
}

/**
 * wrapper for SSL_CTX_check_private_key 
 *   - check consistency of a private key with the certificate in a SSL_CTX. 
 * 
 * @param c  a pointer to SSL_CTX.
 * 
 * @return   1 on success, otherwise failed.
 */
inline int check_private_key(const SSL_CTX* c) {
    return SSL_CTX_check_private_key(c);
}

/**
 * shutdown a ssl connection 
 *   - It MUST be called in the coroutine that performed the I/O operation. 
 *   - This function will check the result of SSL_get_error(), if SSL_ERROR_SYSCALL 
 *     or SSL_ERROR_SSL was returned, SSL_shutdown() will not be called. 
 *   - See documents here: 
 *     https://www.openssl.org/docs/man1.1.0/man3/SSL_get_error.html 
 * 
 *   - NOTE: Is it necessary to shutdown the SSL connection on TCP? Why not close the 
 *     underlying TCP connection directly?
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: 3000. 
 * 
 * @return    1 on success, 
 *           <0 on any error, call ssl::strerror() to get the error message. 
 */
int shutdown(SSL* s, int ms=3000);

/**
 * wait for a TLS/SSL client to initiate a handshake 
 *   - It MUST be called in the coroutine that performed the I/O operation. 
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: -1. 
 * 
 * @return    1 on success, a TLS/SSL connection has been established. 
 *          <=0 on any error, call ssl::strerror() to get the error message. 
 */
int accept(SSL* s, int ms=-1);

/**
 * initiate the handshake with a TLS/SSL server
 *   - It MUST be called in the coroutine that performed the I/O operation. 
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: -1. 
 * 
 * @return    1 on success, a TLS/SSL connection has been established. 
 *          <=0 on any error, call ssl::strerror() to get the error message. 
 */
int connect(SSL* s, int ms=-1);

/**
 * recv data from a TLS/SSL connection 
 *   - It MUST be called in a coroutine. 
 * 
 * @param s    a pointer to SSL.
 * @param buf  a pointer to a buffer to recieve the data.
 * @param n    size of buf.
 * @param ms   timeout in milliseconds, -1 for never timeout. 
 *             default: -1. 
 * 
 * @return    >0  bytes recieved. 
 *           <=0  an error occured, call ssl::strerror() to get the error message. 
 */
int recv(SSL* s, void* buf, int n, int ms=-1);

/**
 * recv n bytes from a TLS/SSL connection 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are recieved. 
 * 
 * @param s    a pointer to SSL.
 * @param buf  a pointer to a buffer to recieve the data.
 * @param n    bytes to be recieved.
 * @param ms   timeout in milliseconds, -1 for never timeout. 
 *             default: -1. 
 * 
 * @return     n on success (all n bytes has been recieved). 
 *           <=0 on any error, call ssl::strerror() to get the error message. 
 */
int recvn(SSL* s, void* buf, int n, int ms=-1);

/**
 * send data on a TLS/SSL connection 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are sent. 
 * 
 * @param s    a pointer to SSL.
 * @param buf  a pointer to a buffer of the data to be sent.
 * @param n    size of buf.
 * @param ms   timeout in milliseconds, -1 for never timeout. 
 *             default: -1. 
 * 
 * @return     n on success (all n bytes has been sent out), 
 *           <=0 on any error, call ssl::strerror() to get the error message. 
 */
int send(SSL* s, const void* buf, int n, int ms=-1);

/**
 * check whether a previous API call has timed out 
 *   - When an API with a timeout like ssl::recv returns, ssl::timeout() can be called 
 *     to check whether it has timed out. 
 * 
 * @return  true if timed out, otherwise false.
 */
inline bool timeout() { return co::timeout(); }

/**
 * ssl server based on coroutine 
 *   - It is designed to work with TCP. 
 */
class Server {
  public:
    Server();
    ~Server();

    /**
     * set a callback for handling a ssl connection 
     *   - The user MUST call ssl::free_ssl() to free the SSL when the connection was closed.
     * 
     * @param f  either a pointer to void f(SSL*), or a reference of std::function<void(SSL*)>.
     */
    void on_connection(std::function<void(SSL*)>&& f) {
        _on_ssl_connection = std::move(f);
    }

    /**
     * set a callback for handling a ssl connection 
     *   - The user MUST call ssl::free_ssl() to free the SSL when the connection was closed.
     * 
     * @param f  pointer to a method with a parameter of type SSL* in class T.
     * @param o  pointer to an object of class T.
     */
    template<typename T>
    void on_connection(void (T::*f)(SSL*), T* o) {
        _on_ssl_connection = std::bind(f, o, std::placeholders::_1);
    }

    /**
     * start the ssl server 
     *   - The server will loop in a coroutine, and it will not block the calling thread. 
     *   - The user MUST call on_connection() to set a connection callback before start() 
     *     was called. 
     * 
     * @param ip    server ip, either an ipv4 or ipv6 address. 
     *              if ip is NULL or empty, "0.0.0.0" will be used by default.
     * @param port  server port.
     * @param key   path of private key file.
     * @param ca    path of certificate file.
     */
    void start(const char* ip, int port, const char* key, const char* ca);

  private:
    tcp::Server _tcp_serv;
    SSL_CTX* _ctx;
    std::function<void(SSL*)> _on_ssl_connection;

    void on_tcp_connection(sock_t fd);

    DISALLOW_COPY_AND_ASSIGN(Server);
};

/**
 * ssl client based on coroutine 
 *   - It is designed to work with TCP. 
 */
class Client {
  public:
    Client(const char* serv_ip, int serv_port);
    ~Client() { this->disconnect(); }

    int recv(void* buf, int n, int ms=-1) {
        return ssl::recv(_ssl, buf, n, ms);
    }

    int recvn(void* buf, int n, int ms=-1) {
        return ssl::recvn(_ssl, buf, n, ms);
    }

    int send(const void* buf, int n, int ms=-1) {
        return ssl::send(_ssl, buf, n, ms);
    }

    bool connected() const { return _ssl != NULL; }

    /**
     * connect to the ssl server 
     *   - It MUST be called in the thread that performed the I/O operation. 
     *
     * @param ms  timeout in milliseconds
     */
    bool connect(int ms);

    /**
     * close the connection 
     *   - It MUST be called in the thread that performed the I/O operation. 
     *   - The underlying tcp connection will also be closed. 
     */
    void disconnect();

    SSL* ssl() const { return _ssl; }

  private:
    tcp::Client _tcp_cli;
    SSL_CTX* _ctx;
    SSL* _ssl;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // ssl

#endif
