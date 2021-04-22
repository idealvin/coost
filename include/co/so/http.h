#pragma once

#include "../fastring.h"
#include <functional>
#include <vector>

namespace so {
namespace http {

enum Version {
    kHTTP10, kHTTP11, kHTTP20,
};

enum Method {
    kGet, kHead, kPost, kPut, kDelete, kOptions,
};

class Base {
  public:
    Base() : _parsing(0), _version(kHTTP11) {}
    ~Base() = default;

    int version() const { return _version; }

    const char* version_str() const {
        static const char* s[] = {
            "HTTP/1.0", "HTTP/1.1",
        };
        return s[_version];
    }

    bool is_version_http10() const { return _version == kHTTP10; }
    bool is_version_http11() const { return _version == kHTTP11; }
    void set_version_http10() { _version = kHTTP10; }
    void set_version_http11() { _version = kHTTP11; }

    const fastring& body() const { return _body; }
    fastring& mutable_body() { return _body; }
    void set_body(fastring&& s) { _body = std::move(s); }
    void set_body(const fastring& s) { _body = s; }
    int body_len() const { return (int) _body.size(); }

    void add_header(fastring&& key, fastring&& val) {
        _headers.push_back(std::move(key));
        _headers.push_back(std::move(val));
    }

    void add_header(const fastring& key, const fastring& val) {
        _headers.push_back(key);
        _headers.push_back(val);
    }

    const fastring& header(const fastring& key) const {
        static const fastring kEmptyString;
        if (_headers.empty()) return kEmptyString;

        const fastring s = key.upper();
        for (size_t i = 0; i < _headers.size(); i += 2) {
            if (_headers[i].upper() == s) return _headers[i + 1];
        }
        return kEmptyString;
    }

    void clear() {
        _parsing = 0;
        _body.clear();
        _headers.clear();
    }

    // ===========================================================
    // for internal use
    // ===========================================================
    void set_parsing() { _parsing = 1; }
    int parsing() const { return _parsing; }

  protected:
    int _parsing;
    int _version;
    fastring _body;
    std::vector<fastring> _headers;
};

class Req : public Base {
  public:
    Req() : _method(kGet) {}
    explicit Req(Method method) : _method(method) {}
    ~Req() = default;

    int method() const { return _method; }

    const char* method_str() const {
        static const char* s[] = {
            "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS"
        };
        return s[_method];
    }

    void set_method_get() { _method = kGet; }
    void set_method_head() { _method = kHead; }
    void set_method_post() { _method = kPost; }
    void set_method_put() { _method = kPut; }
    void set_method_delete() { _method = kDelete; }
    void set_method_options() { _method = kOptions; }

    bool is_method_get() const { return _method == kGet; }
    bool is_method_head() const { return _method == kHead; }
    bool is_method_post() const { return _method == kPost; }
    bool is_method_put() const { return _method == kPut; }
    bool is_method_delete() const { return _method == kDelete; }
    bool is_method_options() const { return _method == kOptions; }

    const fastring& url() const { return _url; }
    void set_url(fastring&& s) { _url = std::move(s); }
    void set_url(const fastring& s) { _url = s; }

    void clear() {
        Base::clear();
        _url.clear();
    }

    fastring str() const;
    fastring dbg() const;

  private:
    int _method;
    fastring _url;
};

class Res : public Base {
  public:
    Res() : _status(200) {}
    ~Res() = default;

    int status() const { return _status; }

    static const char* status_str(int err) {
        static const char** s = create_status_table();
        return (100 <= err && err <= 599) ? s[err] : s[500];
    }

    const char* status_str() const {
        return Res::status_str(_status);
    }

    void set_status(int status) {
        _status = (100 <= status && status <= 599) ? status : 500;
    }

    void clear() {
        Base::clear();
        _status = 200;
    }

    fastring str() const;
    fastring dbg() const;

  private:
    int _status;

    static const char** create_status_table();
};

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
     * 
     * @param ip    server ip, either an ipv4 or ipv6 address, default: "0.0.0.0".
     * @param port  server port, default: 80.
     */
    void start(const char* ip="0.0.0.0", int port=80);

    /**
     * start a https server 
     *   - openssl required by this method. 
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

class Client {
  public:
    /**
     * the constructor 
     *   - serv_url: 
     *     - "127.0.0.1:8080" 
     *     - "http://127.0.0.1" 
     *     - "https://127.0.0.1:7777" 
     *     - "https://github.com" 
     *     - "https://[::1]:7777/" 
     * 
     * @param serv_url  url of the http or https server.
     */
    explicit Client(const char* serv_url);
    ~Client();

    void call(const Req& req, Res& res) {
        if (_https) {
            this->call_https(req, res);
        } else {
            this->call_http(req, res);
        }
    }

    fastring get(fastring&& url) {
        return on_method(kGet, std::move(url));
    }

    fastring get(const fastring& url) {
        return on_method(kGet, url);
    }

    fastring post(fastring&& url, fastring&& body) {
        return on_method(kPost, std::move(url), std::move(body));
    }

    fastring post(const fastring& url, const fastring& body) {
        return on_method(kPost, url, body);
    }

    fastring head(fastring&& url) {
        return on_method(kHead, std::move(url));
    }

    fastring head(const fastring& url) {
        return on_method(kHead, url);
    }

    fastring del(fastring&& url) {
        return on_method(kDelete, std::move(url));
    }

    fastring del(const fastring& url) {
        return on_method(kDelete, url);
    }

    fastring put(fastring&& url, fastring&& body) {
        return on_method(kPut, std::move(url), std::move(body));
    }

    fastring put(const fastring& url, const fastring& body) {
        return on_method(kPut, url, body);
    }

    fastring options() {
        Req req(kOptions);
        Res res;
        req.set_url("*");
        this->call(req, res);
        return res.header("Allow");
    }

  private:
    void call_http(const Req& req, Res& res);
    void call_https(const Req& req, Res& res);

    fastring on_method(Method method, fastring&& url) {
        Req req(method);
        Res res;
        req.set_url(std::move(url));
        this->call(req, res);
        return std::move(res.mutable_body());
    }

    fastring on_method(Method method, const fastring& url) {
        Req req(method);
        Res res;
        req.set_url(url);
        this->call(req, res);
        return std::move(res.mutable_body());
    }

    fastring on_method(Method method, fastring&& url, fastring&& body) {
        Req req(method);
        Res res;
        req.set_url(std::move(url));
        req.set_body(std::move(body));
        this->call(req, res);
        return std::move(res.mutable_body());
    }

    fastring on_method(Method method, const fastring& url, const fastring& body) {
        Req req(method);
        Res res;
        req.set_url(url);
        req.set_body(body);
        this->call(req, res);
        return std::move(res.mutable_body());
    }

  private:
    void* _p;
    bool _https;

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
