#pragma once

#include "../fastring.h"
#include <functional>
#include <vector>

#ifdef HAS_LIBCURL
#include <curl/curl.h>
#endif

namespace http {

/**
 * ===========================================================================
 * HTTP client 
 *   - libcurl & zlib required. 
 *   - openssl required for https. 
 * ===========================================================================
 */

#ifdef HAS_LIBCURL
struct curl_ctx;

/**
 * http client for coroutine programming
 *   - This is an implement based on libcurl. Internally, each client owns a multi
 *     handle and an easy handle. Multi handle in libcurl is a little complicated.
 *     It is recommended to read the documents below to have a better understanding
 *     of the multi handle:
 *       https://everything.curl.dev/libcurl/drive/multi-socket
 *
 *   - NOTE: http::Client will not url-encode the url passed in. The user may call
 *     url_encode() in co/hash/url.h to encode the url before passing it to a http
 *     request, if necessary.
 */
class Client {
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
     *
     * @param url  This url will appear in the request line, it MUST begins with '/'.
     */
    void put(const char* url, const char* data, size_t size);

    void put(const char* url, const char* s) {
        this->put(url, s, strlen(s));
    }

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

    /**
     * get curl easy handle owned by this client
     */
    CURL* easy_handle() const;

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

    /**
     * get error message of the current request
     */
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
    const char* header() const;

    /**
     * get body of the current HTTP response
     *   - NOTE: Contents of the body will be cleared when the next request was performed.
     *   - The result may not be null-terminated, call body_size() to get the size.
     *
     * @return  a pointer to the response body.
     */
    const char* body() const;

    /**
     * get body size of the current HTTP response
     */
    size_t body_size() const;

    /**
     * close the http connection
     *   - Once close() is called, this client can not be used anymore.
     */
    void close();

  private:
    void do_io();
    void set_headers();
    void append_header(const char* s);
    const char* make_url(const char* url);

  private:
    curl_ctx* _ctx;
};
#endif


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

class Req {
  public:
    Req() = default;
    ~Req() = default;

    Version version()        const { return (Version)_version; }
    bool is_method_get()     const { return _method == kGet; }
    bool is_method_head()    const { return _method == kHead; }
    bool is_method_post()    const { return _method == kPost; }
    bool is_method_put()     const { return _method == kPut; }
    bool is_method_delete()  const { return _method == kDelete; }
    bool is_method_options() const { return _method == kOptions; }
    const fastring& url()    const { return _url; }
    const char* body()       const { return _buf->data() + _body; }
    size_t body_size()       const { return _body_size; }

    const char* header(const char* key) const;

    void clear() { _buf = 0; _url.clear(); _s.clear(); _headers.clear(); }

  private:
    int _version;
    int _method;
    size_t _body;
    size_t _body_size;
    fastring* _buf;
    fastring _url;
    fastring _s;
    std::vector<size_t> _headers;

    friend class ServerImpl;
    int parse(fastring* buf);
};

class Res {
  public:
    Res() : _version(kHTTP11), _status(200) {}
    ~Res() = default;

    void set_version(Version v) { _version = v; }
    void set_status(int status) { _status = status; }

    void add_header(const char* key, const char* val) {
        _header.append(key).append(": ").append(val).append("\r\n");
    }

    void set_body(const void* s, size_t n) {
        _body.clear();
        _body.append(s, n);
    }

    void set_body(const char* s) {
        this->set_body(s, strlen(s));
    }

    void clear() {
        _version = kHTTP11; _status = 200;
        _header.clear(); _body.clear();
    }

    fastring str() const;
    size_t body_size() const { return _body.size(); }

  private:
    int _version;
    int _status;
    fastring _header;
    fastring _body;
};

/**
 * http server based on coroutine 
 *   - support both http and https, openssl required for https. 
 *   - support both ipv4 and ipv6. 
 *   - NOTE: http::Server will not url-decode the url in the request. The user may 
 *     call url_decode() in co/hash/url.h to decode the url, if necessary. 
 */
class Server {
  public:
    Server();
    ~Server();

    /**
     * set a callback for handling http request 
     * 
     * @param f  a pointer to void xxx(const Req&, Res&), or 
     *           a reference of std::function<void(const Req&, Res&)>
     */
    void on_req(std::function<void(const Req&, Res&)>&& f);

    /**
     * set a callback for handling http request 
     * 
     * @param f  a pointer to a method in class T.
     * @param o  a pointer to an object of class T.
     */
    template<typename T>
    void on_req(void (T::*f)(const Req&, Res&), T* o) {
        on_req(std::bind(f, o, std::placeholders::_1, std::placeholders::_2));
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
void easy(const char* root_dir=".", const char* ip="0.0.0.0", int port=80);

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
void easy(const char* root_dir, const char* ip, int port, const char* key, const char* ca);

} // so
