#include "co/unitest.h"
#include "co/co.h"
#include "co/cout.h"

namespace test {

int gc = 0;
int gd = 0;

struct TestChan {
    explicit TestChan(int v = 0) : v(v) {
        atomic_inc(&gc, mo_relaxed);
    }

    TestChan(const TestChan& c) : v(c.v) {
        atomic_inc(&gc, mo_relaxed);
    }

    TestChan(TestChan&& c) : v(c.v) {
        c.v = 0;
        atomic_inc(&gc, mo_relaxed);
    }

    ~TestChan() {
        if (v) v = 0;
        atomic_inc(&gd, mo_relaxed);
    }

    int v;
};

struct queue {
    static const int N = 12;
    struct _memb : co::clink {
        size_t size;
        uint8 rx;
        uint8 wx;
        void* q[];
    };

    _memb* _make_memb() {
        _memb* m = (_memb*) co::alloc(sizeof(_memb) + N * sizeof(void*));
        m->size = 0;
        m->rx = 0;
        m->wx = 0;
        return m;
    }

    queue() noexcept : _m(0) {}

    ~queue() {
        for (auto h = _q.front(); h;) {
            const auto m = (_memb*)h;
            h = h->next;
            co::free(m, sizeof(_memb) + N * sizeof(void*));
            co::print("free memb: ", m);
        }
    }

    size_t size() const { return _m ? _m->size : 0; }
    bool empty() const { return this->size() == 0; }

    void push_back(void* x) {
        _memb* m = (_memb*) _q.back();
        if (!m || m->wx == N) {
            m = this->_make_memb();
            _q.push_back(m);
        }
        m->q[m->wx++] = x;
        ++_m->size;
    }

    void* pop_front() {
        void* x = 0;
        if (_m && _m->rx < _m->wx) {
            x = _m->q[_m->rx++];
            --_m->size;
            if (_m->rx == _m->wx) {
                _m->rx = _m->wx = 0;
                if (_q.back() != _m) {
                    _memb* const m = (_memb*) _q.pop_front();
                    _m->size = m->size;
                    co::free(m, sizeof(_memb) + N * sizeof(void*));
                }
            }
        }
        return x;
    }

    union {
        _memb* _m;
        co::clist _q;
    };
};

struct Buffer {
    struct H {
        uint32 cap;
        uint32 size;
        char p[];
    };

    constexpr Buffer() noexcept : _h(0) {}
    ~Buffer() { this->reset(); }

    const char* data() const noexcept { return _h ? _h->p : 0; }
    uint32 size() const noexcept { return _h ? _h->size : 0; }
    uint32 capacity() const noexcept { return _h ? _h->cap : 0; }
    void clear() noexcept { if (_h) _h->size = 0; }

    void reset() {
        if (_h) {
            co::free(_h, _h->cap + 8);
            _h = 0;
        }
    }

    void append(const void* p, size_t size) {
        const uint32 n = (uint32)size;
        if (!_h) {
            _h = (H*) co::alloc(size + 8); assert(_h);
            _h->cap = n;
            _h->size = 0;
            goto lable;
        }

        if (_h->cap < _h->size + n) {
            const uint32 o = _h->cap;
            _h->cap += (o >> 1) + n;
            _h = (H*) co::realloc(_h, o + 8, _h->cap + 8); assert(_h);
            goto lable;
        }

      lable:
        memcpy(_h->p + _h->size, p, n);
        _h->size += n;
    }

    H* _h;
};

struct Coroutine {
    Coroutine() { memset(this, 0, sizeof(*this)); }
    ~Coroutine() { buf.~Buffer(); }

    uint32 id; // coroutine id
    void* ctx; // coroutine context, points to the stack bottom
    union {
        Buffer buf;   // for saving stack data of this coroutine
        void* pbuf;
    };
    void* x[5];
};

class CoroutinePool {
  public:
    static const int E = 5;
    static const int N = 1 << E; // max coroutines per block
    static const int M = 4;

    CoroutinePool()
        : _c(0), _o(0), _v(M), _use_count(M) {
        _v.resize(M);
        _use_count.resize(M);
    }

    ~CoroutinePool() {
        for (size_t i = 0; i < _v.size(); ++i) {
            if (_v[i]) ::free(_v[i]);
        }
        _v.clear();
    }

    Coroutine* pop() {
        int id = 0;
        if (!_v0.empty()) { id = _v0.pop_back(); goto reuse; }
        if (!_vc.empty()) { id = _vc.pop_back(); goto reuse; }
        if (_o < N) goto newco;
        _c = !_blks.empty() ? *_blks.begin() : _c + 1;
        if (!_blks.empty()) _blks.erase(_blks.begin());
        _o = 0;

      newco:
        {
            if (_c < _v.size()) {
                if (!_v[_c]) _v[_c] = (Coroutine*) ::calloc(N, sizeof(Coroutine));
            } else {
                const int c = god::align_up<M>(_c + 1);
                _v.resize(c);
                _use_count.resize(c);
                _v[_c] = (Coroutine*) ::calloc(N, sizeof(Coroutine));
            }

            auto& co = _v[_c][_o];
            co.id = (_c << E) + _o++;
            _use_count[_c]++;
            return &co;
        }

      reuse:
        {
            const int q = id >> E;
            const int r = id & (N - 1);
            auto& co = _v[q][r];
            co.ctx = 0;
            _use_count[q]++;
            return &co;
        }
    }

    void push(Coroutine* co) {
        const int id = co->id;
        const int q = id >> E;
        if (q == 0) {
            if (_v0.capacity() == 0) _v0.reserve(N);
            _v0.push_back(id);
            goto end;
        }
        if (q == _c) {
            if (_vc.capacity() == 0) _vc.reserve(N);
            _vc.push_back(id);
            goto end;
        }

      end:
        if (--_use_count[q] == 0) {
            ::free(_v[q]);
            _v[q] = 0;
            if (q != _c) {
                _blks.insert(q);
            } else {
                _vc.clear();
                _o = 0;
                if (!_blks.empty() && *_blks.begin() < _c) {
                    _blks.insert(_c);
                    _c = *_blks.begin();
                    _blks.erase(_blks.begin());
                }
            }
        }
    }

    Coroutine& operator[](int i) const {
        const int q = i >> E;
        const int r = i & (N - 1);
        return _v[q][r];
    }

  private:
    int _c; // current block
    int _o; // offset in the current block [0, S)
    co::vector<Coroutine*> _v;
    co::vector<int> _use_count;
    co::vector<int> _v0; // id of coroutine in _v[0]
    co::vector<int> _vc; // id of coroutine in _v[_c]
    co::set<int> _blks; // blocks available
};

DEF_test(co) {
    int v = 0;

    DEF_case(wait_group) {
        co::wait_group wg;
        wg.add(8);
        for (int i = 0; i < 7; ++i) {
            go([wg, &v]() {
                atomic_inc(&v);
                wg.done();
            });
        }

        std::thread([wg, &v]() {
            atomic_inc(&v);
            wg.done();
        }).detach();

        wg.wait();
        EXPECT_EQ(v, 8);
        v = 0;
    }

    DEF_case(queue) {
        queue q;
        EXPECT(q.empty());

        int a, b, c, d;
        q.push_back(&a);
        q.push_back(&b);
        q.push_back(&c);
        q.push_back(&d);
        EXPECT_EQ(q.size(), 4);

        EXPECT_EQ(q.pop_front(), &a);
        EXPECT_EQ(q.pop_front(), &b);
        EXPECT_EQ(q.size(), 2);

        for (int i = 0; i < 12; ++i) q.push_back(&a);
        EXPECT_EQ(q.size(), 14);

        for (int i = 0; i < 12; ++i) q.pop_front();
        EXPECT_EQ(q.size(), 2);

        EXPECT_EQ(q.pop_front(), &a);
        EXPECT_EQ(q.pop_front(), &a);
        EXPECT(q.empty());
    }

    DEF_case(Buffer) {
        Buffer b;
        EXPECT_EQ(b.capacity(), 0);

        b.append("hello", 5);
        EXPECT_EQ(b.capacity(), 5);
        EXPECT_EQ(b.size(), 5);

        b.append("world", 5);
        EXPECT_EQ(b.size(), 10);
        EXPECT_EQ(fastring(b.data(), b.size()), "helloworld");

        const uint32 c = b.capacity();
        b.clear();
        EXPECT_EQ(b.size(), 0);
        EXPECT_EQ(b.capacity(), c);

        b.reset();
        EXPECT_EQ(b.capacity(), 0);
    }

    DEF_case(CoroutinePool) {
        CoroutinePool p;
        typedef Coroutine* pco;
        pco a, b, c, d, e, f, o, x, y, z;
        a = p.pop();
        b = p.pop();
        EXPECT_EQ(a->id, 0);
        EXPECT_EQ(b->id, 1);

        p.push(b);
        c = p.pop();
        EXPECT_EQ(c->id, 1);

        b = p.pop();
        EXPECT_EQ(b->id, 2);

        co::vector<Coroutine*> ac;
        for (int i = 0; i < 29; ++i) ac.push_back(p.pop());
        d = p.pop();
        e = p.pop();
        f = p.pop();
        EXPECT_EQ(d->id, 32);
        EXPECT_EQ(e->id, 33);
        EXPECT_EQ(f->id, 34);

        p.push(e); // push 33
        p.push(f); // push 34
        e = p.pop();
        f = p.pop();
        EXPECT_EQ(e->id, 34);
        EXPECT_EQ(f->id, 33);

        for (int i = 0; i < 29; ++i) ac.push_back(p.pop());
        for (int i = 0; i < 64; ++i) ac.push_back(p.pop());
        x = p.pop();
        y = p.pop();
        EXPECT_EQ(x->id, 128);
        EXPECT_EQ(y->id, 129);

        p.push(b); // push 2
        z = p.pop();
        EXPECT_EQ(z->id, 2);
        o = p.pop();
        EXPECT_EQ(o->id, 130);

        p.push(d);
        p.push(e);
        p.push(f);
        for (int i = 29; i < 58; ++i) {
            p.push(ac[i]);
        }

        p.push(x);
        p.push(y);
        p.push(o);

        o = p.pop();
        EXPECT_EQ(o->id, 32);
    }

    DEF_case(mutex) {
        co::mutex m;
        co::wait_group wg;

        m.lock();
        EXPECT_EQ(m.try_lock(), false);
        m.unlock();
        EXPECT_EQ(m.try_lock(), true);
        m.unlock();

        wg.add(16);
        for (int i = 0; i < 12; ++i) {
            go([wg, m, &v]() {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            });
        }

        for (int i = 0; i < 4; ++i) {
            std::thread([wg, m, &v]() {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            }).detach();
        }

        wg.wait();
        EXPECT_EQ(v, 16);
        v = 0;

        wg.add(16);
        auto s = co::next_sched();
        for (int i = 0; i < 16; ++i) {
            s->go([wg, m, &v] {
                co::mutex_guard g(m);
                ++v;
                wg.done();
            });
        }
        wg.wait();
        EXPECT_EQ(v, 16);
        v = 0;
    }

    DEF_case(event) {
        {
            co::event ev;
            co::wait_group wg(2);
            v = 777;

            go([wg, ev, &v]() {
                v = 0;
                ev.wait();
                if (v == 1) v = 2;
                wg.done();
            });

            go([wg, ev, &v]() {
                while (v != 0) co::sleep(1);
                v = 1;
                ev.signal();
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 2);

            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
            ev.signal();
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);

            v = 0;
            wg.add(8);
            for (int i = 0; i < 7; ++i) {
                go([wg, ev, &v]() {
                    atomic_inc(&v);
                    ev.wait();
                    atomic_dec(&v);
                    wg.done();
                });
            }
            std::thread([wg, ev, &v]() {
                atomic_inc(&v);
                ev.wait();
                atomic_dec(&v);
                wg.done();
            }).detach();

            while (v != 8) co::sleep(1);
            co::sleep(1);
            ev.signal();
            wg.wait();
            EXPECT_EQ(v, 0);
            EXPECT_EQ(ev.wait(0), false);

            wg.add(2);
            go([wg, ev, &v]() {
                atomic_inc(&v);
                while (v < 2) co::sleep(1);
                ev.wait(1);
                atomic_inc(&v);
                wg.done();
            });
            std::thread([wg, ev, &v]() {
                atomic_inc(&v);
                while (v < 2) co::sleep(1);
                ev.wait(1);
                atomic_inc(&v);
                wg.done();
            }).detach();

            while (v < 4) co::sleep(1);
            ev.signal();
            wg.wait();
            EXPECT_EQ(v, 4);
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), false);
        }
        {
            co::event ev(true, true); // manual reset
            co::wait_group wg(1);

            go([wg, ev, &v]() {
                if (ev.wait(32)) {
                    ev.reset();
                    v = 1;
                }
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 1);
            v = 0;

            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
            ev.signal();
            EXPECT_EQ(ev.wait(0), true);
            EXPECT_EQ(ev.wait(0), true);

            ev.reset();
            EXPECT_EQ(ev.wait(0), false);
            EXPECT_EQ(ev.wait(0), false);
        }
    }

    DEF_case(chan) {
        {
            co::chan<int> ch;
            co::wait_group wg(2);

            go([wg, ch]() {
                ch << 23;
                wg.done();
            });

            go([wg, ch, &v]() {
                ch >> v;
                wg.done();
            });

            wg.wait();
            EXPECT_EQ(v, 23);
            v = 0;
        }

        {
            fastring s("hello");
            fastring t("again");
            auto ps = s.data();
            auto pt = t.data();

            co::chan<fastring> ch(4, 8);
            ch << s;
            ch << std::move(t);
            EXPECT(ch);
            EXPECT(ch.done());

            EXPECT_EQ(s, "hello");
            EXPECT_EQ(t.capacity(), 0);

            fastring x;
            ch >> x;
            EXPECT_EQ(x, "hello");
            EXPECT_NE(x.data(), ps);

            ch >> x;
            EXPECT_EQ(x, "again");
            EXPECT_EQ(x.data(), pt);

            ch << s << s << s << s;
            EXPECT(ch.done());

            ch << s;
            EXPECT(!ch.done());

            ch.close();
            EXPECT(!ch);

            int i = 0;
            do {
                ch >> x;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 4);
            EXPECT_EQ(x, "hello");
        }

        {
            std::string s("hello");
            std::string t("again");

            co::chan<std::string> ch(4, 8);
            ch << s;
            ch << std::move(t);
            EXPECT(ch);
            EXPECT(ch.done());

            EXPECT_EQ(s, "hello");

            std::string x;
            ch >> x;
            EXPECT_EQ(x, "hello");

            ch >> x;
            EXPECT_EQ(x, "again");

            ch << s << s << s << s;
            EXPECT(ch.done());

            ch << s;
            EXPECT(!ch.done());

            ch.close();
            EXPECT(!ch);

            int i = 0;
            do {
                ch >> x;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 4);
            EXPECT_EQ(x, "hello");
        }

        {
            TestChan x(7);
            co::chan<TestChan> ch(4, 8);
            ch << x;
            EXPECT_EQ(x.v, 7);

            TestChan y;
            ch >> y;
            EXPECT_EQ(y.v, 7);

            ch << std::move(x);
            EXPECT_EQ(x.v, 0);

            y.v = 0;
            ch >> y;
            EXPECT_EQ(y.v, 7);

            // ch is full after this
            ch << y << y << y << y;

            co::wait_group wg(2);
            auto s = co::next_sched();
            s->go([ch, wg]() {
                TestChan x(3);
                ch << x;
                wg.done();
            });
            s->go([ch, wg]() {
                TestChan r;
                ch >> r;
                wg.done();
            });
            wg.wait();

            ch >> y >> y >> y;
            EXPECT_EQ(y.v, 7);

            ch >> y;
            EXPECT_EQ(y.v, 3);

            y.v = 0;
            ch >> y;
            EXPECT(!ch.done()); // timeout
            EXPECT_EQ(y.v, 0);

            wg.add(2);
            s->go([ch, wg, &y]() {
                do {
                    ch >> y;
                } while (!ch.done());
                wg.done();
            });
            s->go([ch, wg]() {
                ch << TestChan(8);
                wg.done();
            });
            wg.wait();
            EXPECT_EQ(y.v, 8);

            wg.add(2);
            std::thread([ch, wg, &y]() {
                do {
                    ch >> y;
                } while (!ch.done());
                wg.done();
            }).detach();

            go([ch, wg]() {
                ch << TestChan(7);
                wg.done();
            });
            wg.wait();
            EXPECT_EQ(y.v, 7);

            int i = 0;
            do {
                ch >> y;
                if (ch.done()) ++i;
            } while (ch.done());
            EXPECT_EQ(i, 0);

            y.v = 0;
            wg.add(3);
            s->go([ch, wg, &y]() {
                do {
                    ch >> y;
                } while (ch);
                wg.done();
            });
            s->go([ch, wg]() {
                TestChan y;
                do {
                    ch >> y;
                } while (ch);
                wg.done();
            });
            s->go([ch, wg]() {
                ch.close();
                wg.done();
            });

            wg.wait();
            EXPECT(!ch);
            EXPECT_EQ(y.v, 0);
        }

        {
            TestChan x(7);
            co::chan<TestChan> ch(4);

            co::wait_group wg1(1);
            co::wait_group wg2(2);
            ch << x << x << x << x;

            auto s = co::next_sched();
            s->go([ch, wg2, &x]() {
                ch << x;
                wg2.done();
            });
            s->go([ch, wg2]() {
                TestChan x(7);
                ch << x;
                wg2.done();
            });
            s->go([ch, wg1]() {
                ch.close();
                wg1.done();
            });

            wg1.wait();
            EXPECT(!ch);

            int i = 0;
            do {
                ch >> x;
                if (ch.done()) ++i;
            } while (ch.done());

            wg2.wait();
            EXPECT_EQ(i, 6);
        }

        EXPECT_NE(gc, 0);
        EXPECT_NE(gd, 0);
        EXPECT_EQ(gc, gd);
    }

    DEF_case(pool) {
        co::pool p(
            []() { return (void*) co::make<int>(0); },
            [](void* p) { co::del((int*)p); },
            8192
        );

        int n = co::sched_num();
        auto& scheds = co::scheds();
        co::vector<int> vi(n, 0);

        co::wait_group wg;
        wg.add(n);

        for (int i = 0; i < n; ++i) {
            scheds[i]->go([wg, p, i]() {
                co::pool_guard<int> g(p);
                *g = i;
                wg.done();
            });
        }

        wg.wait();

        wg.add(n);
        for (int i = 0; i < n; ++i) {
            scheds[i]->go([wg, p, i, &vi]() {
                int* x = (int*) p.pop();
                vi[i] = *x;
                p.push(x);
                wg.done();
            });
        }

        wg.wait();
        for (size_t i = 0; i < vi.size(); ++i) {
            EXPECT_EQ(vi[i], i);
        }

        p.clear();
    }
}

} // test
