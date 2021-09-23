#ifndef _WIN32

#include "co/fs.h"
#include "./co/hook.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#ifdef _FILE_OFFSET_BITS
#define _OLD_FILE_OFFSET_BITS _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace fs {

bool exists(const char* path) {
    struct stat attr;
    return ::lstat(path, &attr) == 0;
}

bool isdir(const char* path) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return false;
    return S_ISDIR(attr.st_mode);
}

int64 mtime(const char* path) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return -1;
    return attr.st_mtime;
}

int64 fsize(const char* path) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return -1;
    return attr.st_size;
}

// p = false  ->  mkdir
// p = true   ->  mkdir -p
bool mkdir(const char* path, bool p) {
    if (!p) return ::mkdir(path, 0755) == 0;

    const char* s = strrchr(path, '/');
    if (s == 0) return ::mkdir(path, 0755) == 0;

    fastring parent(path, s - path);
    
    if (fs::exists(parent.c_str())) {
        return ::mkdir(path, 0755) == 0;
    } else {
        return fs::mkdir(parent.c_str(), true) && ::mkdir(path, 0755) == 0;
    }
}

// rf = false  ->  rm or rmdir
// rf = true   ->  rm -rf
bool remove(const char* path, bool rf) {
    if (!fs::exists(path)) return true;

    if (!rf) {
        if (fs::isdir(path)) return ::rmdir(path) == 0;
        return ::unlink(path) == 0;
    } else {
        fastring cmd(strlen(path) + 9);
        cmd.append("rm -rf \"").append(path).append('"');
        FILE* f = popen(cmd.c_str(), "w");
        if (f == NULL) return false;
        return pclose(f) != -1;
    }
}

bool rename(const char* from, const char* to) {
    return ::rename(from, to) == 0;
}

bool symlink(const char* dst, const char* lnk) {
    fs::remove(lnk);
    return ::symlink(dst, lnk) == 0;
}

#define nullfd -1

namespace xx {
int open(const char* path, char mode) {
    if (mode == 'r') {
        return ::open(path, O_RDONLY);
    } else if (mode == 'a') {
        return ::open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else if (mode == 'w') {
        return ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else if (mode == 'm') {
        return ::open(path, O_WRONLY | O_CREAT, 0644);
    } else {
        return nullfd;
    }
}
} // xx

struct fctx {
    int fd;
    fastring path;
};

file::~file() {
    if (!_p) return;
    this->close();
    delete (fctx*) _p;
    _p = 0;
}

file::operator bool() const {
    fctx* p = (fctx*) _p;
    return p && p->fd != nullfd;
}


const fastring& file::path() const {
    fctx* p = (fctx*) _p;
    if (p) return p->path;
    static char kEmptyPath[sizeof(fastring)] = { 0 };
    return *(fastring*)kEmptyPath;
}

bool file::open(const char* path, char mode) {
    // make sure CO_RAW_API(close, read, write) are not NULL
    static bool kHookInit = []() {
        if (CO_RAW_API(close) == 0) ::close(-1);
        if (CO_RAW_API(read) == 0)  { auto r = ::read(-1, 0, 0);  (void)r; }
        if (CO_RAW_API(write) == 0) { auto r = ::write(-1, 0, 0); (void)r; }
        return true;
    }();

    this->close();
    fctx* p = (fctx*) _p;
    if (!p) _p = (p = new fctx);
    p->path = path;
    return (p->fd = xx::open(path, mode)) != nullfd;
}

void file::close() {
    fctx* p = (fctx*) _p;
    if (!p || p->fd == nullfd) return;
    while (CO_RAW_API(close)(p->fd) != 0 && errno == EINTR);
    p->fd = nullfd;
}

void file::seek(int64 off, int whence) {
    static int seekfrom[3] = { SEEK_SET, SEEK_CUR, SEEK_END };
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        whence = seekfrom[whence];
        ::lseek(p->fd, off, whence);
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
        auto r = CO_RAW_API(read)(p->fd, c, toread);
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
        auto r = CO_RAW_API(write)(p->fd, c, towrite);
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
#undef _FILE_OFFSET_BITS
#ifdef _OLD_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS _OLD_FILE_OFFSET_BITS
#undef _OLD_FILE_OFFSET_BITS
#endif

} // namespace fs

#endif
