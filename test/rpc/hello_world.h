#pragma once

#include "base/rpc.h"
#include "base/hash.h"
#include <unordered_map>

namespace xx {

class HelloWorld : public rpc::Service {
  public:
    typedef void (HelloWorld::*Fun)(const Json&, Json&);

    HelloWorld() {
        _methods[hash64("hello")] = &HelloWorld::hello;
        _methods[hash64("world")] = &HelloWorld::world;
    }

    virtual ~HelloWorld() {}

    virtual void process(const Json& req, Json& res) {
        Json& method = req["method"];
        if (!method.is_string()) {
            res.add_member("err", 400);
            res.add_member("errmsg", "400 req has no method");
            return;
        }

        auto it = _methods.find(hash64(method.get_string(), method.size()));
        if (it == _methods.end()) {
            res.add_member("err", 404);
            res.add_member("errmsg", "404 method not found");
            return;
        }

        (this->*it->second)(req, res);
    }

    void hello(const Json& req, Json& res);

    void world(const Json& req, Json& res);

  private:
    std::unordered_map<uint64, Fun> _methods;
};

} // xx
