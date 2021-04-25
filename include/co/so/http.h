#pragma once

#include "../fastring.h"
#include <functional>
#include <vector>

namespace so {
namespace http {

enum Version {
    kHTTP10, kHTTP11,
};

enum Method {
    kGet, kHead, kPost, kPut, kDelete, kOptions,
};

inline const char* version_str(Version v) {
    static const char* s[] = { "HTTP/1.0", "HTTP/1.1", "HTTP/2.0" };
    return s[v];
}

inline const char* method_str(Method m) {
    static const char* s[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS" };
    return s[m];
}

const char* status_str(int n);

struct Header {
    void add_header(fastring&& key, fastring&& val) {
        headers.push_back(std::move(key));
        headers.push_back(std::move(val));
    }

    void add_header(const fastring& key, const fastring& val) {
        headers.push_back(key);
        headers.push_back(val);
    }

    const fastring& header(const char* key) const;

    std::vector<fastring> headers;
};

class Req {
  public:
    Req()         : _version(kHTTP11), _method(kGet), _from_peer(false) {}
    Req(Method m) : _version(kHTTP11), _method(m), _from_peer(false) {}
    ~Req() = default;

    Version version()          const { return _version; }
    bool is_version_http10()   const { return _version == kHTTP10; }
    bool is_version_http11()   const { return _version == kHTTP11; }
    void set_version_http10()        { _version = kHTTP10; }
    void set_version_http11()        { _version = kHTTP11; }
    void set_version(Version v)      { _version = v; }

    Method method()            const { return _method; }
    bool is_method_get()       const { return _method == kGet; }
    bool is_method_head()      const { return _method == kHead; }
    bool is_method_post()      const { return _method == kPost; }
    bool is_method_put()       const { return _method == kPut; }
    bool is_method_delete()    const { return _method == kDelete; }
    bool is_method_options()   const { return _method == kOptions; }
    void set_method_get()            { _method = kGet; }
    void set_method_head()           { _method = kHead; }
    void set_method_post()           { _method = kPost; }
    void set_method_put()            { _method = kPut; }
    void set_method_delete()         { _method = kDelete; }
    void set_method_options()        { _method = kOptions; }
    void set_method(Method m)        { _method = m; }

    const fastring& url()      const { return _url; }
    void set_url(fastring&& s)       { _url = std::move(s); }
    void set_url(const fastring& s)  { _url = s; }

    const fastring& body()     const { return _body; }
    fastring& body()                 { return _body; }
    int body_size()            const { return (int)_body.size(); }
    void set_body(fastring&& s)      { _body = std::move(s); }
    void set_body(const fastring& s) { _body = s; }

    void add_header(fastring&& k, fastring&& v)           { _header.add_header(std::move(k), std::move(v)); }
    void add_header(const fastring& k, const fastring& v) { _header.add_header(k, v); }
    const fastring& header(const char* k)           const { return _header.header(k); }

    void clear() { _header.headers.clear(); _url.clear(); _body.clear(); _from_peer = false; }

    fastring str(bool dbg=false) const;
    fastring dbg() const { return str(true); }

  private:
    Version _version;
    Method _method;
    Header _header;
    fastring _url;
    fastring _body;
    bool _from_peer; // this req was recieved from a remote peer

    friend class ServerImpl;
    int parse(const char* s, size_t n, int* body_size);
};

class Res {
  public:
    Res() : _version(kHTTP11), _status(200), _from_peer(false) {}
    ~Res() = default;

    Version version()          const { return _version; }
    bool is_version_http10()   const { return _version == kHTTP10; }
    bool is_version_http11()   const { return _version == kHTTP11; }
    void set_version_http10()        { _version = kHTTP10; }
    void set_version_http11()        { _version = kHTTP11; }
    void set_version(Version v)      { _version = v; }

    int status()               const { return _status; }
    void set_status(int status)      { _status = status; }

    const fastring& body()     const { return _body; }
    fastring& body()                 { return _body; }
    int body_size()            const { return (int)_body.size(); }
    void set_body(fastring&& s)      { _body = std::move(s); }
    void set_body(const fastring& s) { _body = s; }

    void add_header(fastring&& k, fastring&& v)           { _header.add_header(std::move(k), std::move(v)); }
    void add_header(const fastring& k, const fastring& v) { _header.add_header(k, v); }
    const fastring& header(const char* k)           const { return _header.header(k); }

    void clear() { _status = 200; _header.headers.clear(); _body.clear(); _from_peer = false; }

    fastring str(bool dbg=false) const;
    fastring dbg() const { return str(true); }

  private:
    Version _version;
    int _status;
    Header _header;
    fastring _body;
    bool _from_peer; // this res was recieved from a remote peer

    friend class Client;
    int parse(const char* s, size_t n, int* body_size);
};

/**
 * http server based on coroutine 
 *   - support both http and https. 
 *   - support both ipv4 and ipv6. 
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

/**
 * http client based on coroutine 
 *   - support both http and https. 
 *   - support both ipv4 and ipv6. 
 *   - It MUST be used in a coroutine, all the public methods MUST be called in a coroutine. 
 */
class Client {
  public:
    /**
     * - serv_url 
     *   - "127.0.0.1"                 http://127.0.0.1:80 
     *   - "https://127.0.0.1"         https://127.0.0.1:443 
     *   - "https://github.com"        https://github.com:443
     *   - "https://[::1]:7777"        enclose the ipv6 address with [] to distinguish it from the port 
     * 
     * @param serv_url  the server url in a form of "protocol://host:port".
     */
    explicit Client(const char* serv_url);
    ~Client();

    /**
     * perform a general http request 
     */
    void call(const Req& req, Res& res) {
        _https ? this->call_https(req, res): this->call_http(req, res);
    }

    /**
     * perform a simple GET request 
     * 
     * @param url  url of the request, it MUST begins with '/'.
     */
    Res& get(fastring&& url)      { return do_req(kGet, std::move(url)); }
    Res& get(const fastring& url) { return do_req(kGet, fastring(url)); }

    /**
     * perform a simple POST request 
     * 
     * @param url   url of the request, it MUST begins with '/'.
     * @param body  body of the request.
     */
    Res& post(fastring&& url, fastring&& body)           { return do_req(kPost, std::move(url), std::move(body)); }
    Res& post(const fastring& url, const fastring& body) { return do_req(kPost, fastring(url), fastring(body)); }

    /**
     * perform a simple DELETE request 
     * 
     * @param url   url of the request, it MUST begins with '/'.
     */
    Res& del(fastring&& url)      { return do_req(kDelete, std::move(url)); }
    Res& del(const fastring& url) { return do_req(kDelete, fastring(url)); }

    /**
     * perform a simple PUT request 
     * 
     * @param url   url of the request, it MUST begins with '/'.
     * @param body  body of the request.
     */
    Res& put(fastring&& url, fastring&& body)           { return do_req(kPut, std::move(url), std::move(body)); }
    Res& put(const fastring& url, const fastring& body) { return do_req(kPut, fastring(url), fastring(body)); }

    /**
     * perform a simple OPTIONS request 
     *   - The user can check res.header("Allow") to get the result. 
     */
    Res& options() {
        if (_req) { _req->clear(); _res->clear(); }
        else { _req = new Req; _res = new Res; }
        _req->set_method_options();
        _req->set_url("*");
        this->call(*_req, *_res);
        return *_res;
    }

  private:
    void call_http(const Req& req, Res& res);
    void call_https(const Req& req, Res& res);

    Res& do_req(Method method, fastring&& url) {
        if (_req) { _req->clear(); _res->clear(); }
        else { _req = new Req; _res = new Res; }
        _req->set_method(method);
        _req->set_url(std::move(url));
        this->call(*_req, *_res);
        return *_res;
    }

    Res& do_req(Method method, fastring&& url, fastring&& body) {
        if (_req) { _req->clear(); _res->clear(); }
        else { _req = new Req; _res = new Res; }
        _req->set_method(method);
        _req->set_url(std::move(url));
        _req->set_body(std::move(body));
        this->call(*_req, *_res);
        return *_res;
    }

  private:
    void* _p;
    bool _https;
    Req* _req;
    Res* _res;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // http

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

namespace http = so::http;
