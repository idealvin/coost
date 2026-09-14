#include <cerrno>
#ifndef _WIN32

#include "co/fs.h"
#include "co/mem.h"
#include "close.h"
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

    co::string parent(path, s - path);
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

bool _rmdir(co::string& s) {
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

bool rm(const char* path, bool r) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return true; // not exists
    if (!S_ISDIR(attr.st_mode)) return ::unlink(path) == 0;
    if (!r) return ::rmdir(path) == 0;

    co::string s(path);
    s.trim_right('/');
    return !s.empty() ? _rmdir(s) : false;
}

bool mv(const char* from, const char* to) {
    co::string s(from);
    s.trim_right('/');
    if (s.empty()) return false;

    struct stat attr;
    if (::lstat(to, &attr) != 0 || !S_ISDIR(attr.st_mode)) {
        return ::rename(s.c_str(), to) == 0;
    }

    const char* p = co::memrchr(from, '/', s.size());
    s.clear();
    s.append(to);
    if (!s.ends_with('/')) s.append('/');
    s.append(p ? p + 1 : from);
    return ::rename(from, s.c_str()) == 0;
}

bool symlink(const char* dst, const char* lnk) {
    struct stat attr;
    if (::lstat(lnk, &attr) == 0 && S_ISLNK(attr.st_mode)) {
        ::unlink(lnk);
    }
    return ::symlink(dst, lnk) == 0;
}

#define nullfd -1

struct fctx {
    uint32 n;
    int err;
    int fd;
    int dummy;
};

file::file(size_t n) : _p(0) {
    const size_t x = n + sizeof(fctx) + !n;
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

inline int _open(const char* path, char mode) {
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

bool file::open(const char* path, char mode) {
    this->close();
    if (!path || !*path) return false;

    const uint32 n = (uint32)strlen(path) + 1;
    const uint32 x = n + sizeof(fctx);
    fctx* p = (fctx*)_p;

    if (!p || p->n < x) {
        _p = co::realloc(_p, p ? p->n : 0, x); assert(_p);
        p = (fctx*)_p;
        ::memcpy(p + 1, path, n);
        p->n = x;
    } else {
        ::memcpy(p + 1, path, n);
    }

    p->fd = _open(path, mode);
    return p->fd != nullfd;
}

void file::close() {
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        _close(p->fd);
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

int file::error() {
    fctx* p = (fctx*)_p;
    return (p && p->fd != nullfd) ? p->err : 0;
}

size_t file::read(void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    char* c = (char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G
    p->err = 0;

    while (true) {
        size_t toread = (remain < N ? remain : N);
        auto r = ::read(p->fd, c, toread);
        if (r > 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            c += (size_t)r;
        } else if (r == 0) { /* end of file */
            return n - remain;
        } else {
            if (errno != EINTR) {
                p->err = errno;
                return n - remain;
            }
        }
    }
}

co::string file::read(size_t n) {
    co::string s(n + 1);
    s.resize(this->read(s.data(), n));
    return s;
}

size_t file::write(const void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    const char* c = (const char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G
    p->err = 0;

    while (true) {
        size_t towrite = (remain < N ? remain : N);
        auto r = ::write(p->fd, c, towrite);
        if (r >= 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            c += (size_t)r;
        } else {
            if (errno != EINTR) {
                p->err = errno;
                return n - remain;
            }
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
        ::memcpy(d + 1, path, n);
        d->n = x;
    } else {
        ::memcpy(d + 1, path, n);
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

co::vector<co::string> dir::all() const {
    dctx* d = (dctx*)_p;
    if (!d || !d->d) return co::vector<co::string>();

    co::vector<co::string> r;
    r.reserve(128);
    while ((d->e = ::readdir(d->d))) {
        char* const p = d->e->d_name;
        if (!is_dot_or_dotdot(p)) r.push_back(p);
    }
    return r;
}

co::string dir::iterator::operator*() const {
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

#else
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

__thread co::string* g_s;

inline co::string& cache() {
    return g_s ? *g_s : *(g_s = co::_make_static<co::string>(512));
}

inline int nwc(const char* p) {
    return MultiByteToWideChar(CP_UTF8, 0, p, -1, NULL, 0);
}

inline void utf82wc(const char* p, wchar_t* w, int n) {
    MultiByteToWideChar(CP_UTF8, 0, p, -1, w, n);
}

static wchar_t* widen(const char* p, co::string* x=NULL) {
    co::string& s = x ? *x : cache();
    const int n = nwc(p);
    if (n > 0) {
        s.reserve((size_t)n * sizeof(wchar_t));
        utf82wc(p, (wchar_t*)s.data(), n);
        s.resize((n - 1) * sizeof(wchar_t));
    } else {
        s.reserve(sizeof(wchar_t));
        *(wchar_t*)s.data() = L'\0';
        s.clear();
    }
    return (wchar_t*)s.data();
}

static co::string narrow(const wchar_t* p) {
    co::string s;
    int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
    if (n > 0) {
        s.reserve(n);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, s.data(), n, NULL, NULL);
        s.resize(n - 1);
    }
    return s;
}

const DWORD g_bad_attr = INVALID_FILE_ATTRIBUTES;
const DWORD g_attr_dir = FILE_ATTRIBUTE_DIRECTORY;
const DWORD g_attr_lnk = FILE_ATTRIBUTE_REPARSE_POINT;

inline DWORD _getattr(const wchar_t* path) {
    return GetFileAttributesW(path);
}

inline bool _isdir(const wchar_t* path) {
    const DWORD x = _getattr(path);
    return x != g_bad_attr && (x & g_attr_dir);
}

inline bool _mkdir(const wchar_t* path) {
    return CreateDirectoryW(path, 0);
}

bool exists(const char* path) {
    return _getattr(widen(path)) != g_bad_attr;
}

bool isdir(const char* path) {
    return _isdir(widen(path));
}

int64 mtime(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    BOOL r = GetFileAttributesExW(widen(path), GetFileExInfoStandard, &info);
    if (!r) return -1;
    const FILETIME& wt = info.ftLastWriteTime;
    return ((int64)wt.dwHighDateTime << 32) | wt.dwLowDateTime;
}

int64 fsize(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    BOOL r = GetFileAttributesExW(widen(path), GetFileExInfoStandard, &info);
    if (!r) return -1;
    return ((int64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
}

bool mkdir(const char* path, bool p) {
    if (!p) return _mkdir(widen(path));

    const char* s = strrchr(path, '/');
    if (s == 0) s = strrchr(path, '\\');
    if (s == 0) return _mkdir(widen(path));

    co::string parent(path, s - path);
    if (fs::exists(parent.c_str())) return _mkdir(widen(path));
    return fs::mkdir(parent.c_str(), true) && _mkdir(widen(path));
}

bool mkdir(char* path, bool p) {
    if (!p) return _mkdir(widen(path));

    char* s = (char*) strrchr(path, '/');
    if (s == 0) s = (char*) strrchr(path, '\\');
    if (s == 0) return _mkdir(widen(path));

    const char c = *s;
    *s = '\0';

    if (fs::exists(path)) {
        *s = c;
        return _mkdir(widen(path));
    } 

    const bool x = fs::mkdir(path, true);
    *s = c;
    return x ? _mkdir(widen(path)) : false;
}

inline bool is_dot_or_dotdot(const wchar_t* p) {
    return p[0] == L'.' && (!p[1] || (p[1] == L'.' && !p[2]));
}

inline void _append(co::string& s, const wchar_t* p) {
    const size_t n = (wcslen(p) + 1) * sizeof(wchar_t);
    s.append((char*)p, n);
    s.resize(s.size() - sizeof(wchar_t));
}

static bool _rmdir(co::string& s, wchar_t c) {
    const size_t n = s.size();
    s.append(&c, sizeof(c));
    _append(s, L"*");

    WIN32_FIND_DATAW e;
    HANDLE h = FindFirstFileW((wchar_t*)s.data(), &e);
    if (h == INVALID_HANDLE_VALUE) {
        s.resize(n);
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    do {
        s.resize(n + sizeof(c));
        if (e.dwFileAttributes & g_attr_dir) {
            if (is_dot_or_dotdot(e.cFileName)) continue;
            _append(s, e.cFileName);
            if (!_rmdir(s, c)) goto err;
        } else {
            _append(s, e.cFileName);
            if (!DeleteFileW((wchar_t*)s.data()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
                goto err;
            }
        }
    } while (FindNextFileW(h, &e));

    FindClose(h);
    s.resize(n);
    *(wchar_t*)(s.data() + n) = L'\0';
    return RemoveDirectoryW((wchar_t*)s.data());

err:
    FindClose(h);
    return false;
}

bool rm(const char* path, bool r) {
    const wchar_t* wpath = widen(path);
    const DWORD attr = _getattr(wpath);
    if (attr == g_bad_attr) return true; // not exists
    if (!(attr & g_attr_dir)) return DeleteFileW(wpath);
    if (!r) return RemoveDirectoryW(wpath);

    const wchar_t c = strrchr(path, '/') ? L'/' : L'\\';
    return _rmdir(cache(), c);
}

bool mv(const char* from, const char* to) {
    co::string sfrom, sto;
    wchar_t* x = widen(from, &sfrom);
    wchar_t* y = widen(to, &sto);

    const DWORD a = _getattr(x);
    const DWORD b = _getattr(y);
    if (a == g_bad_attr || b == g_bad_attr) {
        return MoveFileExW(x, y, MOVEFILE_COPY_ALLOWED);
    }
    if (!(b & g_attr_dir)) {
        DWORD f = MOVEFILE_COPY_ALLOWED;
        if (!(a & g_attr_dir)) f |= MOVEFILE_REPLACE_EXISTING;
        return MoveFileExW(x, y, f);
    }

    const char* p = strrchr(from, '/');
    if (!p) p = strrchr(from, '\\');
    const char c = strrchr(to, '/') ? '/' : '\\';
    co::string s(to);
    if (!s.ends_with(c)) s.append(c);
    s.append(p ? p + 1 : from);

    y = widen(s.c_str(), &sto);
    const DWORD w = _getattr(y);
    if (w == g_bad_attr) {
        return MoveFileExW(x, y, MOVEFILE_COPY_ALLOWED);
    }

    if (!(w & g_attr_dir)) {
        DWORD f = MOVEFILE_COPY_ALLOWED;
        if (!(a & g_attr_dir)) f |= MOVEFILE_REPLACE_EXISTING;
        return MoveFileExW(x, y, f);
    }

    if (a & g_attr_dir) RemoveDirectoryW(y); // remove dir y if it is empty
    return MoveFileExW(x, y, MOVEFILE_COPY_ALLOWED);
}

bool symlink(const char* dst, const char* lnk) {
    co::string sdst, slnk;
    wchar_t* x = widen(dst, &sdst);
    wchar_t* y = widen(lnk, &slnk);
    const DWORD a = _getattr(y);
    if (a != g_bad_attr && (a & g_attr_lnk)) {
        (a & g_attr_dir) ? RemoveDirectoryW(y) : DeleteFileW(y);
    }
    const DWORD d = _isdir(x) ? 1 : 0;
    return CreateSymbolicLinkW(y, x, d);
}

#define nullfd INVALID_HANDLE_VALUE

struct fctx {
    uint32 n;
    int err;
    union {
        HANDLE fd;
        uint64 dummy;
    };
};

file::file(size_t n) : _p(0) {
    const size_t x = n + sizeof(fctx) + !n;
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

inline HANDLE _open_file(const char* path, char mode) {
    wchar_t* s = widen(path);
    switch (mode) {
        case 'r':
            return CreateFileW(s, GENERIC_READ, 7, 0, OPEN_EXISTING, 0, 0);
        case 'a':
            return CreateFileW(s, FILE_APPEND_DATA, 7, 0, OPEN_ALWAYS, 0, 0);
        case 'w':
            return CreateFileW(s, GENERIC_WRITE, 7, 0, CREATE_ALWAYS, 0, 0);
        case 'm':
            return CreateFileW(s, GENERIC_WRITE, 7, 0, OPEN_ALWAYS, 0, 0);
        case '+':
            return CreateFileW(s, GENERIC_READ | GENERIC_WRITE, 7, 0, OPEN_ALWAYS, 0, 0);
        default:
            return nullfd;
    }
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
        ::memcpy(p + 1, path, n);
        p->n = x;
    } else {
        ::memcpy(p + 1, path, n);
    }

    p->fd = _open_file(path, mode);
    return p->fd != nullfd;
}

void file::close() {
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        CloseHandle(p->fd);
        p->fd = nullfd;
    }
}

static int g_seekfrom[3] = { FILE_BEGIN, FILE_CURRENT, FILE_END };

void file::seek(int64 off, int whence) {
    fctx* p = (fctx*)_p;
    if (p && p->fd != nullfd) {
        if (off < (1LL << 31)) {
            SetFilePointer(p->fd, (LONG)off, 0, g_seekfrom[whence]);
        } else {
            LARGE_INTEGER li;
            li.QuadPart = off;
            SetFilePointer(p->fd, li.LowPart, &li.HighPart, g_seekfrom[whence]);
        }
    }
}

int file::error() {
    fctx* p = (fctx*)_p;
    return (p && p->fd != nullfd) ? p->err : 0;
}

size_t file::read(void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    char* c = (char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G
    p->err = 0;

    while (true) {
        DWORD r = 0;
        DWORD toread = (DWORD)(remain < N ? remain : N);
        if (ReadFile(p->fd, c, toread, &r, 0) == TRUE) {
            remain -= r;
            if (r < toread || remain == 0) return n - remain;
            c += r;
        } else {
            p->err = GetLastError();
            return n - remain;
        }
    }
}

co::string file::read(size_t n) {
    co::string s(n + 1);
    s.resize(this->read((void*)s.data(), n));
    return s;
}

size_t file::write(const void* s, size_t n) {
    fctx* p = (fctx*)_p;
    if (!p || p->fd == nullfd) return 0;

    const char* c = (const char*)s;
    size_t remain = n;
    const size_t N = 1u << 30; // 1G
    p->err = 0;

    while (true) {
        DWORD r = 0;
        DWORD towrite = (DWORD)(remain < N ? remain : N);
        if (WriteFile(p->fd, c, towrite, &r, 0) == TRUE) {
            remain -= r;
            if (r < towrite || remain == 0) return n - remain;
            c += r;
        } else {
            p->err = GetLastError();
            return n - remain;
        }
    }
}

#undef nullfd

struct dctx {
    size_t n;
    HANDLE d;
    WIN32_FIND_DATAW e;
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
        ::memcpy(d + 1, path, n);
        d->n = x;
    } else {
        ::memcpy(d + 1, path, n);
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
    d->d = FindFirstFileW(widen(p), &d->e); 
    p[n] = '\0';
    return d->d != INVALID_HANDLE_VALUE;
}

void dir::close() {
    dctx* p = (dctx*)_p;
    if (p && p->d != INVALID_HANDLE_VALUE) {
        FindClose(p->d);
        p->d = INVALID_HANDLE_VALUE;
    }
}

const char* dir::path() const {
    return _p ? ((char*)_p + sizeof(dctx)) : "";
}

co::vector<co::string> dir::all() const {
    dctx* d = (dctx*)_p;
    if (!d || d->d == INVALID_HANDLE_VALUE) return co::vector<co::string>();

    co::vector<co::string> r;
    r.reserve(128);
    do {
        wchar_t* const p = d->e.cFileName;
        if (!is_dot_or_dotdot(p)) {
            r.push_back(narrow(p));
        }
    } while (FindNextFileW(d->d, &d->e));
    return r;
}

co::string dir::iterator::operator*() const {
    assert(_p);
    return narrow(((dctx*)_p)->e.cFileName);
}

dir::iterator& dir::iterator::operator++() {
    dctx* d = (dctx*)_p;
    if (d) {
        BOOL x;
        assert(d->d != INVALID_HANDLE_VALUE);
        while ((x = ::FindNextFileW(d->d, &d->e))) {
            if (!is_dot_or_dotdot(d->e.cFileName)) break;
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
            if (!is_dot_or_dotdot(d->e.cFileName)) break;
        } while ((x = ::FindNextFileW(d->d, &d->e)));
        if (x) return dir::iterator(_p);
    }
    return dir::iterator(NULL);
}

} // namespace fs

#endif
