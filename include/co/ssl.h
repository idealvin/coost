#pragma once

#include "def.h"

namespace ssl {

typedef void S; // SSL
typedef void C; // SSL_CTX

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
__coapi const char* strerror(S* s = 0);

/**
 * create a SSL_CTX
 * 
 * @param c  's' for server, 'c' for client.
 * 
 * @return   a pointer to SSL_CTX on success, NULL on error.
 */
__coapi C* new_ctx(char c);

/**
 * create a SSL_CTX for server
 * 
 * @return  a pointer to SSL_CTX on success, NULL on error.
 */
inline C* new_server_ctx() { return new_ctx('s'); }

/**
 * create a SSL_CTX for client
 * 
 * @return  a pointer to SSL_CTX on success, NULL on error.
 */
inline C* new_client_ctx() { return new_ctx('c'); }

/**
 * wrapper for SSL_CTX_free
 * 
 * @param c  a pointer to SSL_CTX
 */
__coapi void free_ctx(C* c);

/**
 * wrapper for SSL_new
 *   - create a SSL with the given SSL_CTX. 
 * 
 * @param c  a pointer to SSL_CTX.
 * 
 * @return   a pointer to SSL on success, NULL on error.
 */
__coapi S* new_ssl(C* c);

/**
 * wrapper for SSL_free 
 * 
 * @param s  a pointer to SSL
 */
__coapi void free_ssl(S* s);

/**
 * wrapper for SSL_set_fd 
 *   - set a socket fd to SSL. 
 * 
 * @param s   a pointer to SSL.
 * @param fd  a non-blocking socket, also overlapped on windows.
 * 
 * @return    1 on success, 0 on error.
 */
__coapi int set_fd(S* s, int fd);

/**
 * wrapper for SSL_get_fd 
 *   - get the socket fd in a SSL. 
 * 
 * @param s  a pointer to SSL.
 * 
 * @return   a socket fd >= 0 on success, or -1 on error.
 */
__coapi int get_fd(const S* s);

/**
 * wrapper for SSL_CTX_use_PrivateKey_file 
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the private key.
 * 
 * @return      1 on success, otherwise failed.
 */
__coapi int use_private_key_file(C* c, const char* path);

/**
 * wrapper for SSL_CTX_use_certificate_file 
 * 
 * @param c     a pointer to SSL_CTX.
 * @param path  path of a pem file that stores the certificate.
 * 
 * @return      1 on success, otherwise failed.
 */
__coapi int use_certificate_file(C* c, const char* path);

/**
 * wrapper for SSL_CTX_check_private_key 
 *   - check consistency of a private key with the certificate in a SSL_CTX. 
 * 
 * @param c  a pointer to SSL_CTX.
 * 
 * @return   1 on success, otherwise failed.
 */
__coapi int check_private_key(const C* c);

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
__coapi int shutdown(S* s, int ms=3000);

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
__coapi int accept(S* s, int ms=-1);

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
__coapi int connect(S* s, int ms=-1);

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
__coapi int recv(S* s, void* buf, int n, int ms=-1);

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
__coapi int recvn(S* s, void* buf, int n, int ms=-1);

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
__coapi int send(S* s, const void* buf, int n, int ms=-1);

/**
 * check whether a previous API call has timed out 
 *   - When an API with a timeout like ssl::recv returns, ssl::timeout() can be called 
 *     to check whether it has timed out. 
 * 
 * @return  true if timed out, otherwise false.
 */
__coapi bool timeout();

} // ssl
