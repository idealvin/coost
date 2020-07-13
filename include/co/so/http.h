#pragma once

#include "tcp.h"
#include <vector>
#include <functional>

namespace so {
namespace http {

enum Version {
    kHTTP10, kHTTP11,
};

enum Method {
    kGet, kHead, kPost, kPut, kDelete,
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
    ~Req() = default;

    int method() const { return _method; }

    const char* method_str() const {
        static const char* s[] = {
            "GET", "HEAD", "POST", "PUT", "DELETE"
        };
        return s[_method];
    }

    void set_method_get() { _method = kGet; }
    void set_method_head() { _method = kHead; }
    void set_method_post() { _method = kPost; }
    void set_method_put() { _method = kPut; }
    void set_method_delete() { _method = kDelete; }

    bool is_method_get() const { return _method == kGet; }
    bool is_method_head() const { return _method == kHead; }
    bool is_method_post() const { return _method == kPost; }
    bool is_method_put() const { return _method == kPut; }
    bool is_method_delete() const { return _method == kDelete; }

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

class Server : public tcp::Server {
  public:
    typedef std::function<void(const Req&, Res&)> Fun;

    Server(const char* ip, int port);
    virtual ~Server();

    void on_req(Fun&& fun) {
        _on_req = std::move(fun);
    }

    void on_req(const Fun& fun) {
        _on_req = fun;
    }

    void process(const Req& req, Res& res) {
        if (_on_req) {
            _on_req(req, res);
        } else {
            res.set_status(501);
        }
    }

    virtual void start();

  private:
    virtual void on_connection(Connection* conn);

  private:
    int32 _conn_num;
    co::Pool _buffer; // memory buffer for co::recv()
    Fun _on_req;
};

class Client : public tcp::Client {
  public:
    Client(const char* serv_ip, int serv_port);
    virtual ~Client();

    void call(const Req& req, Res& res);
};

} // http

// start a static http server
//   @root_dir: docroot, default is the current directory
//   @ip: server ip
//   @port: default is 80
void easy(const char* root_dir=".", const char* ip="0.0.0.0", int port=80);

} // so

namespace http = so::http;
