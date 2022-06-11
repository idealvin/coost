#pragma once

#include "fastring.h"
#include <functional>

namespace http {

/**
 * ===========================================================================
 * HTTP client 
 *   - libcurl & zlib required. 
 *   - openssl required for https. 
 * ===========================================================================
 */

struct curl_ctx_t;

/**
 * http client for coroutine programming
 *   - NOTE: It will not url-encode the url passed in. Call url_encode() in 
 *     co/hash/url.h to encode the url if necessary.
 */
class __coapi Client {
  public:
    /**
     * initialize a http client with a server url
     *   - If a protocol is not present in the url, http will be used by default.
     *   - If a port is not present in the url, the default port 80 or 443 will be used.
     *   - If the url contains both an ipv6 address and a port, the ip must be enclosed
     *     with [] to distinguish it from the port.
     *   - eg.
     *     "github.com"   "https://github.com"   "http://127.0.0.1:7777"   "http://[::1]:8888"
     *
     * @param serv_url  server url in a form of "protocol://host:port".
     *                  - protocol:  http or https.
     *                  - host:      a domain name, or an ipv4 or ipv6 address.
     *                  - port:      server port.
     */
    explicit Client(const char* serv_url);
    ~Client();

    Client(const Client&) = delete;
    void operator=(const Client&) = delete;

    /**
     * add a HTTP header
     *   - The header will be set into an easy curl handle, which will be reused
     *     in later HTTP requests.
     *
     * @param key  a non-empty string.
     * @param val  the value, an empty string is allowed.
     */
    void add_header(const char* key, const char* val);

    /**
     * add a HTTP header with an integer value
     *   - add_header("Content-Length", 777);
     *
     * @param key  a non-empty string.
     * @param val  an integer value.
     */
    void add_header(const char* key, int val);

    /**
     * remove a HTTP header
     *   - A header will be reused by all the following requests by default. The
     *     user can use this method to remove a header, so it will not appear in
     *     later HTTP requests.
     *
     * @param key  a non-empty string.
     */
    void remove_header(const char* key);

    /**
     * perform a HTTP GET request
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void get(const char* url);

    /**
     * perform a HTTP HEAD request
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void head(const char* url);

    /**
     * perform a HTTP POST request
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void post(const char* url, const char* data, size_t size);

    void post(const char* url, const char* s) {
        this->post(url, s, strlen(s));
    }

    /**
     * perform a HTTP PUT request
     *   - upload a file to the server
     *
     * @param url   This url will appear in the request line, it MUST begins with '/'.
     * @param path  Path of the file to be uploaded.
     */
    void put(const char* url, const char* path);

    /**
     * perform a HTTP DELETE request
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void del(const char* url, const char* data, size_t size);

    void del(const char* url, const char* s) {
        this->del(url, s, strlen(s));
    }

    /**
     * perform a HTTP DELETE request without a body
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void del(const char* url) {
        return this->del(url, "", 0);
    }

    /**
     * set http url
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void set_url(const char* url);

    // get curl easy handle (CURL*) owned by this client
    void* easy_handle() const;

    /**
     * perform a HTTP request
     *   - This method is designed for HTTP request other than GET, HEAD, POST,
     *     PUT, DELETE.
     *   - The user may call set_url() to set a url, and set other options with
     *     the easy handle, then call this method to perform the request.
     */
    void perform();

    /**
     * get response code of the current HTTP request
     *
     * @return  a non-zero value like 200, 404, if a response was recieved from the
     *          server, otherwise 0 will be returned and strerror() can be used to
     *          get the error message.
     */
    int response_code() const;

    // the same as response_code()
    int status() const { return this->response_code(); }

    // get error message of the current request
    const char* strerror() const;

    /**
     * get value of a HTTP header in the current response
     *   - NOTE: Contents of the header will be cleared when the next request was performed.
     *   - The result will be an empty string if the header is not found.
     *
     * @param key  a null terminated string, non-case sensitive.
     *
     * @return     a pointer to a null-terminated string.
     */
    const char* header(const char* key);

    /**
     * get the entire header part of the current HTTP response
     *   - NOTE: Contents of the header will be cleared when the next request was performed.
     *
     * @return  a pointer to a null-terminated string, which contains the response start line.
     */
    const fastring& header() const;

    /**
     * get body of the current HTTP response
     *   - NOTE: Contents of the body will be cleared when the next request was performed.
     */
    const fastring& body() const;

    // Close the connection.
    // Once it is called, the client can't be used until you reset the server url.
    void close();

    // reset server url
    void reset(const char* serv_url);

  private:
    void append_header(const char* s);
    const char* make_url(const char* url);

  private:
    curl_ctx_t* _ctx;
};


/**
 * ===========================================================================
 * HTTP server 
 *   - openssl required for https. 
 *   - only support HTTP/1.0 & HTTP/1.1 at this moment. 
 * ===========================================================================
 */

enum Version {
    kHTTP10, kHTTP11,
};

enum Method {
    kGet, kHead, kPost, kPut, kDelete, kOptions,
};

struct http_req_t;
struct http_res_t;

class __coapi Req {
  public:
    Req() : _p(0) {}
    ~Req();

    Version version()        const { return (Version) ((uint32*)_p)[1]; }
    Method method()          const { return (Method)  ((uint32*)_p)[0]; }
    bool is_method_get()     const { return this->method() == kGet; }
    bool is_method_head()    const { return this->method() == kHead; }
    bool is_method_post()    const { return this->method() == kPost; }
    bool is_method_put()     const { return this->method() == kPut; }
    bool is_method_delete()  const { return this->method() == kDelete; }
    bool is_method_options() const { return this->method() == kOptions; }

    const fastring& url()    const { return *(fastring*)((uint32*)_p + 4); }

    // return a null-terminated value of the header
    const char* header(const char* key) const;

    // return a pointer to the body, which may be not null-terminated, call 
    // body_size() to get the length.
    const char* body() const;

    // get length of the body
    size_t body_size() const { return ((uint32*)_p)[3]; }

  private:
    http_req_t* _p;
};

class __coapi Res {
  public:
    Res() : _p(0) {}
    ~Res();

    /**
     * set response code
     *   - NOTE: it MUST be called before set_body() or body()
     */
    void set_status(int status) { *(uint32*)_p = status; }

    /**
     * add a HTTP header to the response
     *   - NOTE: it MUST be called before set_body() or body()
     *   - 'Content-Length' will be added automatically, no need to add it manually.
     */
    void add_header(const char* key, const char* val);

    // add a header with an integer value
    void add_header(const char* key, int val);

    /**
     * set body of the response
     *   - The body length will be zero if no body was set.
     */
    void set_body(const void* s, size_t n);
    void set_body(const char* s) { this->set_body(s, strlen(s)); }
    void set_body(const fastring& s) { this->set_body(s.data(), s.size()); }

  private:
    http_res_t* _p;
};

/**
 * http server based on coroutine 
 *   - support both http and https, openssl required for https. 
 *   - support both ipv4 and ipv6. 
 *   - NOTE: http::Server will not url-decode the url in the request. The user may 
 *     call url_decode() in co/hash/url.h to decode the url, if necessary. 
 */
class __coapi Server {
  public:
    Server();
    ~Server();

    /**
     * set a callback for handling http request 
     * 
     * @param f  a pointer to void xxx(const Req&, Res&), or 
     *           a reference of std::function<void(const Req&, Res&)>
     */
    Server& on_req(std::function<void(const Req&, Res&)>&& f);

    Server& on_req(const std::function<void(const Req&, Res&)>& f) {
        return this->on_req(std::function<void(const Req&, Res&)>(f));
    }

    /**
     * set a callback for handling http request 
     * 
     * @param f  a pointer to a method in class T.
     * @param o  a pointer to an object of class T.
     */
    template<typename T>
    Server& on_req(void (T::*f)(const Req&, Res&), T* o) {
        return on_req(std::bind(f, o, std::placeholders::_1, std::placeholders::_2));
    }

    /**
     * start a http server 
     *   - It will not block the calling thread. 
     * 
     * @param ip    server ip, either an ipv4 or ipv6 address, default: "0.0.0.0".
     * @param port  server port, default: 80.
     */
    void start(const char* ip="0.0.0.0", int port=80);

    /**
     * start a https server 
     *   - openssl required by this method. 
     *   - It will not block the calling thread. 
     * 
     * @param ip    server ip, either an ipv4 or ipv6 address.
     * @param port  server port.
     * @param key   path of the private key file for ssl.
     * @param ca    path of the certificate file for ssl.
     */
    void start(const char* ip, int port, const char* key, const char* ca);

    /**
     * exit the server gracefully
     *   - Once `exit()` was called, the listening socket will be closed, and new 
     *     connections will not be accepted. Since co v3.0, the server will reset 
     *     previously established connections.
     */
    void exit();

  private:
    void* _p;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // http

namespace so {

/**
 * start a static http server 
 *   - This function will block the calling thread. 
 * 
 * @param root_dir  docroot, default: the current directory.
 * @param ip        server ip, either an ipv4 or ipv6 address, default: "0.0.0.0"
 * @param port      server port, default: 80.
 */
__coapi void easy(const char* root_dir = ".", const char* ip = "0.0.0.0", int port = 80);

/**
 * start a static https server 
 *   - This function will block the calling thread. 
 *   - openssl required by this method. 
 * 
 * @param root_dir  docroot.
 * @param ip        server ip, either an ipv4 or ipv6 address.
 * @param port      server port.
 * @param key       path of the private key file for ssl.
 * @param ca        path of the certificate file for ssl.
 */
__coapi void easy(const char* root_dir, const char* ip, int port, const char* key, const char* ca);

} // so
