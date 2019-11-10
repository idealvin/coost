#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "test.h"
#include "base/def.h"
#include "base/log.h"
#include "base/str.h"
#include "base/fastream.h"
#include "base/thread.h"
#include "base/time.h"
#include "base/co.h"

template<size_t N>
inline size_t cslen(const char(&)[N]) {
    CLOG << "const char (&)[N]";
    return N - 1;
}

// haszero(v) (((v) - 0x01010101UL) & ~(v) & 0x80808080UL)
template<typename T>
inline bool haszero(T v) {
    return ((v - ((T)-1)/0xff) & ~v & (((T)-1) / 0xff * 0x80));
}

// hasvalue(x,n)  (haszero((x) ^ (~0UL/255 * (n))))
template<typename T>
inline bool hasbyte(T v, unsigned char n) {
    return haszero(v ^ (((T)-1) / 0xff * n));
}

void a() {
    char* p = 0;
    //*p = 'c';
    CHECK_EQ(1 + 1, 3);
}

void b() {
    a();
}

void c() {
    b();
}

void f() {
    COUT << "f(): " << now::str();
}

void g() {
    COUT << "g(): " << now::str();
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    CLOG << "hello world";

    CLOG << "now: " << now::str();
    TaskSched s;
    s.run_in(f, 1);
    s.run_every(g, 3);
    s.run_daily(f, 23, 14, 0);

    while (1) {
        //c();
        //go(c);
        //Thread(c).detach();
        while (1) sleep::sec(1024);
    }

    return 0;
}
