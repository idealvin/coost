#ifdef _WIN32
#include "co/fs.h"
#include "co/mem.h"

#ifdef _MSC_VER
#pragma warning (disable:4800)
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs {

bool exists(const char* path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

bool isdir(const char* path) {
    DWORD x = GetFileAttributesA(path);
    return x != INVALID_FILE_ATTRIBUTES && (x & FILE_ATTRIBUTE_DIRECTORY);
}

int64 mtime(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    BOOL r = GetFileAttributesExA(path, GetFileExInfoStandard, &info);
    if (!r) return -1;

    FILETIME& wt = info.ftLastWriteTime;
    return ((int64) wt.dwHighDateTime << 32) | wt.dwLowDateTime;
}

int64 fsize(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    BOOL r = GetFileAttributesExA(path, GetFileExInfoStandard, &info);
    if (!r) return -1;
    return ((int64) info.nFileSizeHigh << 32) | info.nFileSizeLow;
}

// p = false  ->  mkdir
// p = true   ->  mkdir -p
bool mkdir(const char* path, bool p) {
    if (!p) return CreateDirectoryA(path, 0);

    const char* s = strrchr(path, '/');
    if (s == 0) s = strrchr(path, '\\');
    if (s == 0) return CreateDirectoryA(path, 0);

    fastring parent(path, s - path);

    if (fs::exists(parent.c_str())) {
        return CreateDirectoryA(path, 0);
    } else {
        return fs::mkdir(parent.c_str(), true) && CreateDirectoryA(path, 0);
    }
}

bool mkdir(char* path, bool p) {
    if (!p) return CreateDirectoryA(path, 0);

    char* s = (char*) strrchr(path, '/');
    if (s == 0) s = (char*) strrchr(path, '\\');
    if (s == 0) return CreateDirectoryA(path, 0);

    char c = *s;
    *s = '\0';

    if (fs::exists(path)) {
        *s = c;
        return CreateDirectoryA(path, 0);
    } else {
        bool x = fs::mkdir(path, true);
        *s = c;
        return x ? CreateDirectoryA(path, 0) : false;
    }
}

// rf = false  ->  rm or rmdir
// rf = true   ->  rm -rf
bool remove(const char* path, bool rf) {
    if (!fs::exists(path)) return true;

    if (!rf) {
        if (fs::isdir(path)) return RemoveDirectoryA(path);
        return DeleteFileA(path);
    } else {
        fastring cmd(strlen(path) + 12);
        cmd.append("rd /s /q \"").append(path).append('"');
        return system(cmd.c_str()) != -1;
    }
}

bool rename(const char* from, const char* to) {
    return MoveFileA(from, to);
}

bool symlink(const char* dst, const char* lnk) {
    fs::remove(lnk);
    return CreateSymbolicLinkA(lnk, dst, fs::isdir(dst));
}

#define nullfd INVALID_HANDLE_VALUE

namespace xx {
HANDLE open(const char* path, char mode) {
    switch (mode) {
      case 'r':
        return CreateFileA(path, GENERIC_READ, 7, 0, OPEN_EXISTING, 0, 0);
      case 'a':
        return CreateFileA(path, FILE_APPEND_DATA, 7, 0, OPEN_ALWAYS, 0, 0);
      case 'w':
        return CreateFileA(path, GENERIC_WRITE, 7, 0, CREATE_ALWAYS, 0, 0);
      case 'm':
        return CreateFileA(path, GENERIC_WRITE, 7, 0, OPEN_ALWAYS, 0, 0);
      case '+':
        return CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 7, 0, OPEN_ALWAYS, 0, 0);
      default:
        return nullfd;
    }
}
} // xx

struct fctx {
    union {
        uint32 n;
        HANDLE _;
    };
    HANDLE fd;
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
    fctx* p = (fctx*) _p;
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
        CloseHandle(p->fd);
        p->fd = nullfd;
    }
}

void file::seek(int64 off, int whence) {
    static int seekfrom[3] = { FILE_BEGIN, FILE_CURRENT, FILE_END };
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        whence = seekfrom[whence];
        if (off < (1LL << 31)) {
            SetFilePointer(p->fd, (LONG)off, 0, whence);
        } else {
            LARGE_INTEGER li;
            li.QuadPart = off;
            SetFilePointer(p->fd, li.LowPart, &li.HighPart, whence);
        }
    }
}

size_t file::read(void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    char* c = (char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G

    while (true) {
        DWORD r = 0;
        DWORD toread = (DWORD)(remain < N ? remain : N);
        if (ReadFile(p->fd, c, toread, &r, 0) == TRUE) {
            remain -= r;
            if (r < toread || remain == 0) return n - remain;
            c += r;
        } else {
            return n - remain;
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
        DWORD r = 0;
        DWORD towrite = (DWORD)(remain < N ? remain : N);
        if (WriteFile(p->fd, c, towrite, &r, 0) == TRUE) {
            remain -= r;
            if (r < towrite || remain == 0) return n - remain;
            c += r;
        } else {
            return n - remain;
        }
    }
}

#undef nullfd

struct dctx {
    size_t n;
    HANDLE d;
    WIN32_FIND_DATA e;
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

    const char c = strchr(path, '/') ? '/' : '\\';
    const size_t n = strlen(path);
    const size_t x = n + sizeof(dctx) + 3; // append "/*"
    dctx* d = (dctx*)_p;

    if (!d || d->n < x) {
        _p = co::realloc(_p, d ? d->n : 0, x); assert(_p);
        d = (dctx*)_p;
        memcpy(d + 1, path, n);
        d->n = x;
    } else {
        memcpy(d + 1, path, n);
    }

    char* p = (char*)(d + 1);
    if (p[n - 1] != c) {
        p[n] = c;
        p[n + 1] = '*';
        p[n + 2] = '\0';
    } else {
        p[n] = '*';
        p[n + 1] = '\0';
    }
    d->d = ::FindFirstFileA(p, &d->e); 
    p[n] = '\0';
    return d->d != INVALID_HANDLE_VALUE;
}

void dir::close() {
    dctx* p = (dctx*)_p;
    if (p && p->d != INVALID_HANDLE_VALUE) {
        ::FindClose(p->d);
        p->d = INVALID_HANDLE_VALUE;
    }
}

const char* dir::path() const {
    return _p ? ((char*)_p + sizeof(dctx)) : "";
}

co::vector<fastring> dir::all() const {
    dctx* d = (dctx*)_p;
    if (!d || d->d == INVALID_HANDLE_VALUE) return co::vector<fastring>();

    co::vector<fastring> r;
    r.reserve(8);
    do {
        char* const p = d->e.cFileName;
        if (p[0] != '.' || (p[1] && (p[1] != '.' || p[2]))) {
            r.push_back(p);
        }
    } while (::FindNextFileA(d->d, &d->e));
    return r;
}

fastring dir::iterator::operator*() const {
    assert(_p);
    return ((dctx*)_p)->e.cFileName;
}

dir::iterator& dir::iterator::operator++() {
    dctx* d = (dctx*)_p;
    if (d) {
        BOOL x;
        assert(d->d != INVALID_HANDLE_VALUE);
        while ((x = ::FindNextFileA(d->d, &d->e))) {
            char* const p = d->e.cFileName;
            if (p[0] != '.' || (p[1] && (p[1] != '.' || p[2]))) break;
        }
        if (!x) _p = NULL;
    }
    return *this;
}

dir::iterator dir::begin() const {
    dctx* d = (dctx*)_p;
    if (d && d->d != INVALID_HANDLE_VALUE) {
        BOOL x = 1;
        do {
            char* const p = d->e.cFileName;
            if (p[0] != '.' || (p[1] && (p[1] != '.' || p[2]))) break;
        } while ((x = ::FindNextFileA(d->d, &d->e)));
        if (x) return dir::iterator(_p);
    }
    return dir::iterator(NULL);
}

} // namespace fs

#endif
