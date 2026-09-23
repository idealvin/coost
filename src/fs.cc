#ifndef _WIN32
#include "co/fs.h"
#include "close.h"
#include <stdio.h>
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
    if (!s || s == path) return ::mkdir(path, 0755) == 0;

    co::string parent(path, s - path);
    if (fs::exists(parent.c_str())) return ::mkdir(path, 0755) == 0;
    return fs::mkdir(parent.c_str(), true) && ::mkdir(path, 0755) == 0;
}

bool mkdir(char* path, bool p) {
    if (!p) return ::mkdir(path, 0755) == 0;

    char* s = (char*) strrchr(path, '/');
    if (!s || s == path) return ::mkdir(path, 0755) == 0;

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
    struct dirent* de;
    while ((de = ::readdir(d))) {
        if (is_dot_or_dotdot(de->d_name)) continue; // ignore . and ..
        s.resize(n);
        s.append('/').append(de->d_name);
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
    const int e = errno;
    ::closedir(d);
    errno = e;
    return false;
}

bool rm(const char* path, bool r) {
    struct stat attr;
    if (::lstat(path, &attr) != 0) return errno == ENOENT;
    if (!S_ISDIR(attr.st_mode)) return ::unlink(path) == 0;

    // rm / is not allowed 
    if (path[0] == '/' && path[1] == '\0') {
        errno = EPERM;
        return false;
    }

    if (!r) return ::rmdir(path) == 0;

    co::string s(path);
    s.trim_right('/');
    if (!s.empty()) return _rmdir(s);

    // path contains only '/'
    errno = EPERM;
    return false;
}

bool mv(const char* from, const char* to) {
    co::string s(from);
    s.trim_right('/');
    if (*from && s.empty()) { /* from contains only '/' */
        errno = EPERM;
        return false;
    }

    struct stat attr;
    const int r = ::lstat(to, &attr);
    if (r != 0 && errno != ENOENT) return false;

    if (r != 0 || !S_ISDIR(attr.st_mode)) {
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

struct _fctx {
    uint32 n;
    int fd;
};

file::file(size_t n) : _p(0) {
    const size_t x = n + sizeof(_fctx) + !n;
    _p = co::alloc(x);
    runtime_assert(_p);
    _fctx* fctx = (_fctx*)_p;
    fctx->n = (uint32)x;
    fctx->fd = nullfd;
    *(char*)(fctx + 1) = '\0';
}

file::~file() {
    if (_p) {
        this->close();
        co::free(_p, ((_fctx*)_p)->n);
        _p = 0;
    }
}

file::operator bool() const noexcept {
    const auto fctx = (_fctx*)_p;
    return fctx && fctx->fd != nullfd;
}

const char* file::path() const noexcept {
    return _p ? ((char*)_p + sizeof(_fctx)) : "";
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
            errno = EINVAL;
            return nullfd;
    }
}

bool file::open(const char* path, char mode) {
    this->close();
    if (!path || !*path) {
        errno = EINVAL;
        return false;
    }

    const uint32 n = (uint32)strlen(path) + 1;
    const uint32 x = n + sizeof(_fctx);
    _fctx* fctx = (_fctx*)_p;

    if (!fctx || fctx->n < x) {
        _p = co::realloc(_p, fctx ? fctx->n : 0, x);
        runtime_assert(_p);
        fctx = (_fctx*)_p;
        ::memcpy(fctx + 1, path, n);
        fctx->n = x;
    } else {
        ::memcpy(fctx + 1, path, n);
    }

    fctx->fd = _open(path, mode);
    return fctx->fd != nullfd;
}

void file::close() {
    _fctx* fctx = (_fctx*)_p;
    if (fctx && fctx->fd != nullfd) {
        _close(fctx->fd);
        fctx->fd = nullfd;
    }
}

constexpr int g_seekfrom[3] = { SEEK_SET, SEEK_CUR, SEEK_END };

bool file::seek(int64 off, _seekfrom_t whence) {
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        errno = EBADF;
        return false;
    }

    static_assert(sizeof(off_t) == sizeof(int64));
    return ::lseek(fctx->fd, off, g_seekfrom[whence]) != (off_t)-1;
}

size_t file::read(void* s, size_t n) {
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        errno = EBADF;
        return 0;
    }

    errno = 0;
    const size_t N = 1u << 30; // 1G
    char* buf = (char*)s;
    size_t remain = n;

    while (true) {
        auto r = ::read(fctx->fd, buf, remain < N ? remain : N);
        if (r > 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            buf += (size_t)r;
        } else if (r == 0) { /* end of file */
            return n - remain;
        } else {
            if (errno != EINTR) return n - remain;
        }
    }
}

co::string file::read(size_t n) {
    co::string s(n + 1);
    s.resize(this->read(s.data(), n));
    return s;
}

size_t file::write(const void* s, size_t n) {
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        errno = EBADF;
        return 0;
    }

    errno = 0;
    const size_t N = 1u << 30; // 1G
    const char* buf = (const char*)s;
    size_t remain = n;

    while (true) {
        auto r = ::write(fctx->fd, buf, remain < N ? remain : N);
        if (r >= 0) {
            remain -= (size_t)r;
            if (remain == 0) return n;
            buf += (size_t)r;
        } else {
            if (errno != EINTR) return n - remain;
        }
    }
}

#undef nullfd

struct _dctx {
    size_t n;
    DIR* d;
    struct dirent* e;
};

dir::~dir() {
    if (_p) {
        this->close();
        co::free(_p, ((_dctx*)_p)->n);
        _p = 0;
    }
}

bool dir::open(const char* path) {
    this->close();
    if (!path || !*path) {
        errno = EINVAL;
        return false;
    }

    const size_t n = strlen(path) + 1;
    const size_t x = n + sizeof(_dctx);
    _dctx* dctx = (_dctx*)_p;

    if (!dctx || dctx->n < x) {
        _p = co::realloc(_p, dctx ? dctx->n : 0, x);
        runtime_assert(_p);
        dctx = (_dctx*)_p;
        ::memcpy(dctx + 1, path, n);
        dctx->n = x;
    } else {
        ::memcpy(dctx + 1, path, n);
    }

    dctx->d = ::opendir(path);
    dctx->e = nullptr;
    return dctx->d;
}

void dir::close() {
    _dctx* dctx = (_dctx*)_p;
    if (dctx && dctx->d) {
        ::closedir(dctx->d);
        dctx->d = nullptr;
    }
}

const char* dir::path() const noexcept {
    return _p ? ((char*)_p + sizeof(_dctx)) : "";
}

co::vector<co::string> dir::all() const {
    _dctx* dctx = (_dctx*)_p;
    if (!dctx || !dctx->d) return co::vector<co::string>();

    co::vector<co::string> r;
    r.reserve(128);
    while ((dctx->e = ::readdir(dctx->d))) {
        char* const p = dctx->e->d_name;
        if (!is_dot_or_dotdot(p)) r.emplace_back(p);
    }
    return r;
}

co::string dir::iterator::operator*() const {
    runtime_assert(_p);
    return co::string(((_dctx*)_p)->e->d_name);
}

dir::iterator& dir::iterator::operator++() {
    _dctx* dctx = (_dctx*)_p;
    if (dctx) {
        runtime_assert(dctx->d);
        while ((dctx->e = ::readdir(dctx->d))) {
            char* const p = dctx->e->d_name;
            if (!is_dot_or_dotdot(p)) break;
        }
        if (!dctx->e) _p = nullptr;
    }
    return *this;
}

dir::iterator dir::begin() const {
    _dctx* dctx = (_dctx*)_p;
    if (dctx && dctx->d) {
        while ((dctx->e = ::readdir(dctx->d))) {
            char* const p = dctx->e->d_name;
            if (!is_dot_or_dotdot(p)) break;
        }
        if (dctx->e) return dir::iterator(_p);
    }
    return dir::iterator(nullptr);
}

} // namespace fs

#else
#include "co/fs.h"

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
    return MultiByteToWideChar(CP_UTF8, 0, p, -1, nullptr, 0);
}

inline void utf82wc(const char* p, wchar_t* w, int n) {
    MultiByteToWideChar(CP_UTF8, 0, p, -1, w, n);
}

static wchar_t* widen(const char* p, co::string* x=nullptr) {
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
    int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
    if (n > 0) {
        s.reserve(n);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, s.data(), n, nullptr, nullptr);
        s.resize(n - 1);
    }
    return s;
}

constexpr DWORD g_bad_attr = INVALID_FILE_ATTRIBUTES;
constexpr DWORD g_attr_dir = FILE_ATTRIBUTE_DIRECTORY;
constexpr DWORD g_attr_lnk = FILE_ATTRIBUTE_REPARSE_POINT;

inline DWORD _getattr(const wchar_t* path) {
    return GetFileAttributesW(path);
}

inline bool _isdir(const wchar_t* path) {
    const DWORD x = _getattr(path);
    return x != g_bad_attr && (x & g_attr_dir);
}

inline bool _mkdir(const wchar_t* path) {
    return CreateDirectoryW(path, nullptr);
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
    if (!s) s = strrchr(path, '\\');
    if (!s) return _mkdir(widen(path));

    co::string parent(path, s - path);
    if (fs::exists(parent.c_str())) return _mkdir(widen(path));
    return fs::mkdir(parent.c_str(), true) && _mkdir(widen(path));
}

bool mkdir(char* path, bool p) {
    if (!p) return _mkdir(widen(path));

    char* s = (char*) strrchr(path, '/');
    if (!s) s = (char*) strrchr(path, '\\');
    if (!s) return _mkdir(widen(path));

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

    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW((wchar_t*)s.data(), &data);
    if (h == INVALID_HANDLE_VALUE) {
        s.resize(n);
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    do {
        s.resize(n + sizeof(c));
        if (data.dwFileAttributes & g_attr_dir) {
            if (is_dot_or_dotdot(data.cFileName)) continue;
            _append(s, data.cFileName);
            if (!_rmdir(s, c)) goto err;
        } else {
            _append(s, data.cFileName);
            if (!DeleteFileW((wchar_t*)s.data()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
                goto err;
            }
        }
    } while (FindNextFileW(h, &data));

    FindClose(h);
    s.resize(n);
    *(wchar_t*)(s.data() + n) = L'\0';
    return RemoveDirectoryW((wchar_t*)s.data());

err:
    const auto e = GetLastError();
    FindClose(h);
    SetLastError(e);
    return false;
}

bool rm(const char* path, bool r) {
    const wchar_t* wpath = widen(path);
    const DWORD attr = _getattr(wpath);
    if (attr == g_bad_attr) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    if (!(attr & g_attr_dir)) return DeleteFileW(wpath);
    if (!r) return RemoveDirectoryW(wpath);

    const wchar_t c = strrchr(path, '/') ? L'/' : L'\\';
    return _rmdir(cache(), c);
}

bool mv(const char* from, const char* to) {
    co::string sfrom, sto;
    wchar_t* wfrom = widen(from, &sfrom);
    wchar_t* wto = widen(to, &sto);

    const DWORD attr_from = _getattr(wfrom);
    const DWORD attr_to = _getattr(wto);
    if (attr_from == g_bad_attr || attr_to == g_bad_attr) {
        return MoveFileExW(wfrom, wto, MOVEFILE_COPY_ALLOWED);
    }
    if (!(attr_to & g_attr_dir)) {
        DWORD f = MOVEFILE_COPY_ALLOWED;
        if (!(attr_from & g_attr_dir)) f |= MOVEFILE_REPLACE_EXISTING;
        return MoveFileExW(wfrom, wto, f);
    }

    const char* p = strrchr(from, '/');
    if (!p) p = strrchr(from, '\\');
    const char c = strrchr(to, '/') ? '/' : '\\';
    co::string s(to);
    if (!s.ends_with(c)) s.append(c);
    s.append(p ? p + 1 : from);

    wto = widen(s.c_str(), &sto);
    const DWORD attr = _getattr(wto);
    if (attr == g_bad_attr) {
        return MoveFileExW(wfrom, wto, MOVEFILE_COPY_ALLOWED);
    }

    if (!(attr & g_attr_dir)) {
        DWORD f = MOVEFILE_COPY_ALLOWED;
        if (!(attr_from & g_attr_dir)) f |= MOVEFILE_REPLACE_EXISTING;
        return MoveFileExW(wfrom, wto, f);
    }

    if (attr_from & g_attr_dir) RemoveDirectoryW(wto); // remove dir wto if it is empty
    return MoveFileExW(wfrom, wto, MOVEFILE_COPY_ALLOWED);
}

bool symlink(const char* dst, const char* lnk) {
    co::string sdst, slnk;
    wchar_t* wdst = widen(dst, &sdst);
    wchar_t* wlnk = widen(lnk, &slnk);
    const DWORD attr = _getattr(wlnk);
    if (attr != g_bad_attr && (attr & g_attr_lnk)) {
        (attr & g_attr_dir) ? RemoveDirectoryW(wlnk) : DeleteFileW(wlnk);
    }
    const DWORD d = _isdir(wdst) ? 1 : 0;
    return CreateSymbolicLinkW(wlnk, wdst, d);
}

#define nullfd INVALID_HANDLE_VALUE

struct _fctx {
    uint32 n;
    HANDLE fd;
};

file::file(size_t n) : _p(0) {
    const size_t x = n + sizeof(_fctx) + !n;
    _p = co::alloc(x);
    runtime_assert(_p);
    _fctx* fctx = (_fctx*)_p;
    fctx->n = (uint32)x;
    fctx->fd = nullfd;
    *(char*)(fctx + 1) = '\0';
}

file::~file() {
    if (_p) {
        this->close();
        co::free(_p, ((_fctx*)_p)->n);
        _p = 0;
    }
}

file::operator bool() const noexcept {
    _fctx* fctx = (_fctx*) _p;
    return fctx && fctx->fd != nullfd;
}

const char* file::path() const noexcept {
    return _p ? ((char*)_p + sizeof(_fctx)) : "";
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
            co::error(EINVAL);
            return nullfd;
    }
}

bool file::open(const char* path, char mode) {
    this->close();
    if (!path || !*path) {
        co::error(EINVAL);
        return false;
    }

    const uint32 n = (uint32)strlen(path) + 1;
    const uint32 x = n + sizeof(_fctx);
    _fctx* fctx = (_fctx*)_p;

    if (!fctx || fctx->n < x) {
        _p = co::realloc(_p, fctx ? fctx->n : 0, x);
        runtime_assert(_p);
        fctx = (_fctx*)_p;
        ::memcpy(fctx + 1, path, n);
        fctx->n = x;
    } else {
        ::memcpy(fctx + 1, path, n);
    }

    fctx->fd = _open_file(path, mode);
    return fctx->fd != nullfd;
}

void file::close() {
    _fctx* fctx = (_fctx*)_p;
    if (fctx && fctx->fd != nullfd) {
        CloseHandle(fctx->fd);
        fctx->fd = nullfd;
    }
}

constexpr int g_seekfrom[3] = { FILE_BEGIN, FILE_CURRENT, FILE_END };

bool file::seek(int64 off, _seekfrom_t whence) {
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        co::error(EBADF);
        return false;
    }

    LARGE_INTEGER li;
    li.QuadPart = off;
    return SetFilePointerEx(fctx->fd, li, nullptr, g_seekfrom[whence]) != 0;
}

size_t file::read(void* s, size_t n) {
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        co::error(EBADF);
        return 0;
    }

    co::error(0);
    const size_t N = 1u << 30; // 1G
    char* buf = (char*)s;
    size_t remain = n;

    while (true) {
        DWORD r = 0;
        DWORD toread = (DWORD)(remain < N ? remain : N);
        if (ReadFile(fctx->fd, buf, toread, &r, nullptr) == TRUE) {
            remain -= r;
            if (remain == 0 || r < toread) return n - remain;
            buf += r;
        } else {
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
    _fctx* fctx = (_fctx*)_p;
    if (!fctx || fctx->fd == nullfd) {
        co::error(EBADF);
        return 0;
    }

    co::error(0);
    const size_t N = 1u << 30; // 1G
    const char* buf = (const char*)s;
    size_t remain = n;

    while (true) {
        DWORD r = 0;
        DWORD towrite = (DWORD)(remain < N ? remain : N);
        if (WriteFile(fctx->fd, buf, towrite, &r, nullptr) == TRUE) {
            remain -= r;
            if (remain == 0) return n;
            buf += r;
        } else {
            return n - remain;
        }
    }
}

#undef nullfd

struct _dctx {
    size_t n;
    HANDLE d;
    WIN32_FIND_DATAW e;
};

dir::~dir() {
    if (_p) {
        this->close();
        co::free(_p, ((_dctx*)_p)->n);
        _p = 0;
    }
}

bool dir::open(const char* path) {
    this->close();
    if (!path || !*path) {
        co::error(EINVAL);
        return false;
    }

    const char c = strchr(path, '/') ? '/' : '\\';
    const size_t n = strlen(path);
    const size_t x = n + sizeof(_dctx) + 3; // append "/*"
    _dctx* dctx = (_dctx*)_p;

    if (!dctx || dctx->n < x) {
        _p = co::realloc(_p, dctx ? dctx->n : 0, x);
        runtime_assert(_p);
        dctx = (_dctx*)_p;
        ::memcpy(dctx + 1, path, n);
        dctx->n = x;
    } else {
        ::memcpy(dctx + 1, path, n);
    }

    char* p = (char*)(dctx + 1);
    if (p[n - 1] != c) {
        p[n] = c;
        p[n + 1] = '*';
        p[n + 2] = '\0';
    } else {
        p[n] = '*';
        p[n + 1] = '\0';
    }

    const auto h = FindFirstFileW(widen(p), &dctx->e);
    dctx->d = (h != INVALID_HANDLE_VALUE ? h : nullptr);
    p[n] = '\0';
    return dctx->d;
}

void dir::close() {
    _dctx* dctx = (_dctx*)_p;
    if (dctx && dctx->d) {
        FindClose(dctx->d);
        dctx->d = nullptr;
    }
}

const char* dir::path() const noexcept {
    return _p ? ((char*)_p + sizeof(_dctx)) : "";
}

co::vector<co::string> dir::all() const {
    _dctx* dctx = (_dctx*)_p;
    if (!dctx || !dctx->d) return co::vector<co::string>();

    co::vector<co::string> r;
    r.reserve(128);
    do {
        wchar_t* const p = dctx->e.cFileName;
        if (!is_dot_or_dotdot(p)) r.emplace_back(narrow(p));
    } while (FindNextFileW(dctx->d, &dctx->e));
    return r;
}

co::string dir::iterator::operator*() const {
    runtime_assert(_p);
    return narrow(((_dctx*)_p)->e.cFileName);
}

dir::iterator& dir::iterator::operator++() {
    _dctx* dctx = (_dctx*)_p;
    if (dctx) {
        BOOL x;
        runtime_assert(dctx->d);
        while ((x = ::FindNextFileW(dctx->d, &dctx->e))) {
            if (!is_dot_or_dotdot(dctx->e.cFileName)) break;
        }
        if (!x) _p = nullptr;
    }
    return *this;
}

dir::iterator dir::begin() const {
    _dctx* dctx = (_dctx*)_p;
    if (dctx && dctx->d) {
        BOOL x = 1;
        do {
            if (!is_dot_or_dotdot(dctx->e.cFileName)) break;
        } while ((x = ::FindNextFileW(dctx->d, &dctx->e)));
        if (x) return dir::iterator(_p);
    }
    return dir::iterator(nullptr);
}

} // namespace fs

#endif
