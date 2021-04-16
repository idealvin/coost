#pragma once

#ifdef CO_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../co.h"
#include "tcp.h"

namespace so {
namespace ssl {

/**
 * handle ssl error 
 *   - This function will store the entire error queue as a string for the current 
 *     thread, and the openssl error queue will be cleared. 
 *   - See more details here: 
 *     https://en.wikibooks.org/wiki/OpenSSL/Error_handling
 * 
 * @return  a pointer to the error message.
 */
const char* strerror();

/**
 * create a SSL_CTX
 * 
 * @param c  's' for server, 'c' for client.
 * 
 * @return  a pointer to SSL_CTX on success, NULL on error.
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
 * free a SSL_CTX
 */
inline void free_ctx(SSL_CTX* c) { SSL_CTX_free(c); }

/**
 * create a SSL with the given SSL_CTX 
 * 
 * @param c  a pointer to SSL_CTX.
 * 
 * @return a pointer to SSL on success, NULL on error
 */
inline SSL* new_ssl(SSL_CTX* c) { return SSL_new(c); }

/**
 * free a SSL
 */
inline void free_ssl(SSL* s) { SSL_free(s); }

/**
 * set a socket fd to SSL 
 * 
 * @param s   a pointer to SSL.
 * @param fd  a non-blocking socket, also overlapped on windows.
 * 
 * @return 1 on success, 0 on error
 */
inline int set_fd(SSL* s, int fd) { return SSL_set_fd(s, fd); }

/**
 * get the socket fd in a SSL 
 * 
 * @param s   a pointer to SSL.
 * 
 * @return a socket fd >= 0 on success, or -1 on error
 */
inline int get_fd(const SSL* s) { return SSL_get_fd(s); }

/**
 * use private key file
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the private key.
 * 
 * @return      1 on success, otherwise failed
 */
inline int use_private_key_file(SSL_CTX* c, const char* path) {
    return SSL_CTX_use_PrivateKey_file(c, path, SSL_FILETYPE_PEM);
}

/**
 * use certificate file
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the certificate.
 * 
 * @return      1 on success, otherwise failed
 */
inline int use_certificate_file(SSL_CTX* c, const char* path) {
    return SSL_CTX_use_certificate_file(c, path, SSL_FILETYPE_PEM);
}

/**
 * check consistency of a private key with the certificate in a SSL_CTX 
 * 
 * @param c     a pointer to SSL_CTX.
 * 
 * @return      1 on success, otherwise failed
 */
inline int check_private_key(const SSL_CTX* c) {
    return SSL_CTX_check_private_key(c);
}

/**
 * shutdown a ssl connection 
 *   - It MUST be called in the coroutine that performed the IO operation. 
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: -1. 
 * 
 * @return    1   success. 
 *           <0   an error occured, call ssl::strerror() to get the error message. 
 */
int shutdown(SSL* s, int ms=-1);

/**
 * wait for a TLS/SSL client to initiate a handshake 
 *   - It MUST be called in the coroutine that performed the IO operation. 
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: -1. 
 * 
 * @return    1   a TLS/SSL connection has been established. 
 *          <=0   an error occured, call ssl::strerror() to get the error message. 
 */
int accept(SSL* s, int ms=-1);

/**
 * initiate the handshake with a TLS/SSL server
 *   - It MUST be called in the coroutine that performed the IO operation. 
 * 
 * @param s   a pointer to SSL.
 * @param ms  timeout in milliseconds, -1 for never timeout. 
 *            default: -1. 
 * 
 * @return    1   a TLS/SSL connection has been established. 
 *          <=0   an error occured, call ssl::strerror() to get the error message. 
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
 * @return    >0   bytes recieved. 
 *           <=0   an error occured, call ssl::strerror() to get the error message. 
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
 * @return     n   on success (all n bytes has been recieved). 
 *           <=0   an error occured, call ssl::strerror() to get the error message. 
 */
int recvn(SSL* s, void* buf, int n, int ms=-1);

/**
 * send data on a TLS/SSL connection 
 *   - It MUST be called in a coroutine. 
 *   - It blocks until all the n bytes are sent. 
 * 
 * @param s    a pointer to SSL.
 * @param buf  a pointer to the data to be sent.
 * @param n    size of buf.
 * @param ms   timeout in milliseconds, -1 for never timeout. 
 *             default: -1. 
 * 
 * @return     n   on success (all n bytes has been sent out)
 *           <=0   an error occured, call ssl::strerror() to get the error message. 
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

class Server : public tcp::Server {
  public:
    Server(const char* ip, int port, const char* privkey, const char* ca);
    virtual ~Server();

    struct Config {
        int32 recv_timeout;
        int32 send_timeout;
        int32 handshake_timeout;
    };

    /**
     * set ssl handshake timeout 
     *   - If the handshake timeout is not set by the user, a global config 
     *     FLG_ssl_handshake_timeout will be used by default. 
     * 
     * @param ms  timeout in milliseconds.
     */
    void set_handshake_timeout(int32 ms) {
        _config->handshake_timeout = ms;
    }

  private:
    virtual void on_connection(tcp::Connection* conn);

    virtual void on_connection(SSL* s) = 0;

  protected:
    SSL_CTX* _ctx;
    std::unique_ptr<Config> _config;
};

class Client : public tcp::Client {
  public:
    Client(const char* serv_ip, int serv_port);
    virtual ~Client();

    int recv(void* buf, int n, int ms=-1) {
        return ssl::recv(_ssl, buf, n, ms);
    }

    int recvn(void* buf, int n, int ms=-1) {
        return ssl::recvn(_ssl, buf, n, ms);
    }

    int send(const void* buf, int n, int ms=-1) {
        return ssl::send(_ssl, buf, n, ms);
    }

    bool connected() const {
        return _ssl != NULL; 
    }

    /**
     * connect to the ssl server 
     *   - It MUST be called in the thread that performed the IO operation. 
     *
     * @param ms  timeout in milliseconds
     */
    bool connect(int ms);

    /**
     * close the connection 
     *   - It MUST be called in the thread that performed the IO operation. 
     */
    void disconnect();

  protected:
    SSL_CTX* _ctx;
    SSL* _ssl;
};

} // ssl
} // so

namespace ssl = so::ssl;

#endif
