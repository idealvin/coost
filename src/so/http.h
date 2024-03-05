#pragma once

#include "co/fastring.h"

namespace tcp{
struct Connection;
}

namespace http {

struct http_req_t {
    http_req_t() = delete;
    ~http_req_t() = delete;

    void add_header(uint32 k, uint32 v);
    const char* header(const char* key) const;

    void clear() {
        body_size = 0;
        url.clear();
        buf = 0;
        arr_size = 0;
    }

    // DO NOT change orders of the members here.
    uint32 method;
    uint32 version;
    uint32 body; // body:  buf->data() + body
    uint32 body_size;
    fastring url;
    fastring* buf; // http data: | header | \r\n\r\n | body |
    uint32* arr;   // array of header index: [<k,v>]
    uint32 arr_size;
    uint32 arr_cap;
};

struct http_res_t {
    http_res_t() = delete;
    ~http_res_t() = delete;

    void add_header(const char* k, const char* v) {
        if (header.capacity() == 0) header.reserve(128);
        header << k << ": " << v << "\r\n";
    }

    void add_header(const char* k, int v) {
        if (header.capacity() == 0) header.reserve(128);
        header << k << ": " << v << "\r\n";
    }

    bool set_body(const void* s, size_t n);

    void clear() {
        status = 0;
        buf = 0;
        header.clear();
        body_size = 0;

        header_send=0;
        s_err=-1;
        pc_=nullptr;
    }

    //for http long connection session
    //User can call multiple times of 'send_body'. 
    //The 1st call will also pack response Header data and send!
    int send_body(const void* data, size_t n);

    // DO NOT change orders of the members here.
    uint32 status;
    uint32 version;
    fastring* buf;
    fastring header;
    size_t body_size;

    // new members for http long connection session implementation (like 'send_body')
    tcp::Connection* pc_;
    int header_send;
    int s_err;
};

int parse_http_req(fastring* buf, size_t size, http_req_t* req);
void send_error_message(int err, http_res_t* res, void* conn);

} // http
