#ifndef _WIN32

#include "co/fs.h"
#include "co/mem.h"
#include "./co/close.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

namespace fs {

bool exists(const char* path) {
    struct stat attr;
    return ::lstat(path, &attr) == 0;
}

bool isdir(const char* path) {
    struct stat attr;
    return ::lstat(path, &attr) == 0 && S_ISDIR(attr.st_mode);
}

int64 mtime(const char* path) {
    struct stat attr;
    return ::lstat(path, &attr) == 0 ? attr.st_mtime : -1;
}

int64 fsize(const char* path) {
    struct stat attr;
    return ::lstat(path, &attr) == 0 ? attr.st_size : -1;
}

bool mkdir(const char* path, bool p) {
    if (!p) return ::mkdir(path, 0755) == 0;

    const char* s = strrchr(path, '/');
    if (s == 0 || s == path) return ::mkdir(path, 0755) == 0;

    fastring parent(path, s - path);
    if (fs::exists(parent.c_str())) return ::mkdir(path, 0755) == 0;
    return fs::mkdir(parent.c_str(), true) && ::mkdir(path, 0755) == 0;
}

bool mkdir(char* path, bool p) {
    if (!p) return ::mkdir(path, 0755) == 0;

    char* s = (char*) strrchr(path, '/');
    if (s == 0 || s == path) return ::mkdir(path, 0755) == 0;

    *s = '\0';
    if (fs::exists(path)) {
        *s = '/';
        return ::mkdir(path, 0755) == 0;
    } else {
        const bool x = fs::mkdir(path, true);
        *s = '/';
        return x ? ::mkdir(path, 0755) == 0 : false;
    }
}

inline bool is_dot_or_dotdot(const char* p) {
    return p[0] == '.' && (!p[1] || (p[1] == '.' && !p[2]));
}

bool _rmdir(fastring& s) {
    DIR* d = ::opendir(s.c_str());
    if (!d) return errno == ENOENT;

    const size_t n = s.size();
    struct dirent* e;
    while ((e = ::readdir(d))) {
        if (is_dot_or_dotdot(e->d_name)) continue; // ignore . and ..
        s.resize(n);
        s.append('/').append(e->d_name);
        if (fs::isdir(s.c_str())) {
            if (!_rmdir(s)) goto err;
        } else {
            if (::unlink(s.c_str()) != 0 && errno != ENOENT) goto err;
        }
    }

    ::closedir(d);
    s.resize(n);
    return ::rmdir(s.c_str()) == 0;

  err:
    ::closedir(d);
    return false;
}

bool remove(const char* path, bool r) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return true; // not exists
    if (!S_ISDIR(attr.st_mode)) return ::unlink(path) == 0;
    if (!r) return ::rmdir(path) == 0;

    fastring s(path);
    return _rmdir(s);
}

bool mv(const char* from, const char* to) {
    struct stat attr;
    if (::lstat(to, &attr) != 0 || !S_ISDIR(attr.st_mode)) {
        return ::rename(from, to) == 0;
    }

    const char* p = strrchr(from, '/');
    fastring s(to);
    if (!s.ends_with('/')) s.append('/');
    s.append(p ? p + 1 : from);
    return ::rename(from, s.c_str()) == 0;
}

bool rename(const char* from, const char* to) {
    return ::rename(from, to) == 0;
}

bool symlink(const char* dst, const char* lnk) {
    struct stat attr;
    if (::lstat(lnk, &attr) == 0 && S_ISLNK(attr.st_mode)) {
        ::unlink(lnk);
    }
    return ::symlink(dst, lnk) == 0;
}

#define nullfd -1

namespace xx {
int open(const char* path, char mode) {
    switch (mode) {
      case 'r':
        return ::open(path, O_RDONLY);
      case 'a':
        return ::open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
      case 'w':
        return ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      case 'm':
        return ::open(path, O_WRONLY | O_CREAT, 0644);
      case '+':
        return ::open(path, O_RDWR | O_CREAT, 0644);
      default:
        return nullfd;
    }
}
} // xx

struct fctx {
    uint32 n;
    int fd;
};

file::file(size_t n) : _p(0) {
    const size_t x = n + sizeof(fctx) + 1;
    _p = co::alloc(x); assert(_p);
    fctx* p = (fctx*)_p;
    p->n = (uint32)x;
    p->fd = nullfd;
    *(char*)(p + 1) = '\0';
}

file::~file() {
    if (_p) {
        this->close();
        co::free(_p, ((fctx*)_p)->n);
        _p = 0;
    }
}

file::operator bool() const {
    fctx* p = (fctx*)_p;
    return p && p->fd != nullfd;
}

const char* file::path() const {
    return _p ? ((char*)_p + sizeof(fctx)) : "";
}

bool file::open(const char* path, char mode) {
    this->close();
    if (!path || !*path) return false;

    const uint32 n = (uint32)strlen(path) + 1;
    const uint32 x = n + sizeof(fctx);
    fctx* p = (fctx*)_p;

    if (!p || p->n < x) {
        _p = co::realloc(_p, p ? p->n : 0, x); assert(_p);
        p = (fctx*)_p;
        memcpy(p + 1, path, n);
        p->n = x;
    } else {
        memcpy(p + 1, path, n);
    }

    p->fd = xx::open(path, mode);
    return p->fd != nullfd;
}

void file::close() {
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        _close_nocancel(p->fd);
        p->fd = nullfd;
    }
}

static int g_seekfrom[3] = { SEEK_SET, SEEK_CUR, SEEK_END };

void file::seek(int64 off, int whence) {
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        ::lseek(p->fd, off, g_seekfrom[whence]);
    }
}

size_t file::read(void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    char* c = (char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G

    while (true) {
        size_t toread = (remain < N ? remain : N);
        auto r = __sys_api(read)(p->fd, c, toread);
        if (r > 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            c += (size_t)r;
        } else if (r == 0) { /* end of file */
            return n - remain;
        } else {
            if (errno != EINTR) return n - remain;
        }
    }
}

fastring file::read(size_t n) {
    fastring s(n + 1);
    s.resize(this->read((void*)s.data(), n));
    return s;
}

size_t file::write(const void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    const char* c = (const char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G

    while (true) {
        size_t towrite = (remain < N ? remain : N);
        auto r = __sys_api(write)(p->fd, c, towrite);
        if (r >= 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            c += (size_t)r;
        } else {
            if (errno != EINTR) return n - remain;
        }
    }
}

#undef nullfd

struct dctx {
    size_t n;
    DIR* d;
    struct dirent* e;
};

dir::~dir() {
    if (_p) {
        this->close();
        co::free(_p, ((dctx*)_p)->n);
        _p = 0;
    }
}

bool dir::open(const char* path) {
    this->close();
    if (!path || !*path) return false;

    const size_t n = strlen(path) + 1;
    const size_t x = n + sizeof(dctx);
    dctx* d = (dctx*)_p;

    if (!d || d->n < x) {
        _p = co::realloc(_p, d ? d->n : 0, x); assert(_p);
        d = (dctx*)_p;
        memcpy(d + 1, path, n);
        d->n = x;
    } else {
        memcpy(d + 1, path, n);
    }

    d->d = ::opendir(path);
    d->e = NULL;
    return d->d;
}

void dir::close() {
    dctx* d = (dctx*)_p;
    if (d && d->d) {
        ::closedir(d->d);
        d->d = NULL;
    }
}

const char* dir::path() const {
    return _p ? ((char*)_p + sizeof(dctx)) : "";
}

co::vector<fastring> dir::all() const {
    dctx* d = (dctx*)_p;
    if (!d || !d->d) return co::vector<fastring>();

    co::vector<fastring> r(8);
    while ((d->e = ::readdir(d->d))) {
        char* const p = d->e->d_name;
        if (!is_dot_or_dotdot(p)) r.push_back(p);
    }
    return r;
}

fastring dir::iterator::operator*() const {
    assert(_p);
    return ((dctx*)_p)->e->d_name;
}

dir::iterator& dir::iterator::operator++() {
    dctx* d = (dctx*)_p;
    if (d) {
        assert(d->d);
        while ((d->e = ::readdir(d->d))) {
            char* const p = d->e->d_name;
            if (!is_dot_or_dotdot(p)) break;
        }
        if (!d->e) _p = NULL;
    }
    return *this;
}

dir::iterator dir::begin() const {
    dctx* d = (dctx*)_p;
    if (d && d->d) {
        while ((d->e = ::readdir(d->d))) {
            char* const p = d->e->d_name;
            if (!is_dot_or_dotdot(p)) break;
        }
        if (d->e) return dir::iterator(_p);
    }
    return dir::iterator(NULL);
}

} // namespace fs

#endif
