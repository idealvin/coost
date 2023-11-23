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

__thread fastring* g_s;

inline fastring& cache() {
    return g_s ? *g_s : *(g_s = co::_make_static<fastring>(512));
}

const int W = sizeof(wchar_t);
typedef wchar_t* PWC;

inline int nwc(const char* p) {
    return MultiByteToWideChar(CP_UTF8, 0, p, -1, NULL, 0);
}

inline void utf82wc(const char* p, wchar_t* w, int n) {
    MultiByteToWideChar(CP_UTF8, 0, p, -1, w, n);
}

static wchar_t* widen(const char* p) {
    fastring& s = cache();
    const int n = nwc(p);
    if (n > 0) {
        s.reserve(n * W);
        utf82wc(p, (PWC)s.data(), n);
        s.resize((n - 1) * W);
    } else {
        *(PWC)s.data() = 0;
        s.clear();
    }
    return (PWC)s.data();
}

static void widen(const char* p, const char* q, PWC* x, PWC* y) {
    fastring& s = cache();
    const int m = nwc(p);
    const int n = nwc(q);
    s.reserve((m + n + !m + !n) * W);

    const PWC px = (PWC)s.data();
    const PWC py = px + (m > 0 ? m : 1);
    m > 0 ? utf82wc(p, px, m) : (void)(*px = 0);
    n > 0 ? utf82wc(q, py, n) : (void)(*py = 0);
    *x = px;
    *y = py;
}

static fastring narrow(const wchar_t* p) {
    fastring s;
    int n = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
    if (n > 0) {
        s.reserve(n);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, (char*)s.data(), n, NULL, NULL);
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

    fastring parent(path, s - path);
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
    } else {
        const bool x = fs::mkdir(path, true);
        *s = c;
        return x ? _mkdir(widen(path)) : false;
    }
}

inline void pathcat(fastring& s, wchar_t c, const wchar_t* p) {
    const wchar_t z = L'\0';
    const size_t n = wcslen(p) * sizeof(wchar_t);
    s.append(&c, sizeof(c)).append((char*)p, n).append(&z, sizeof(z));
    s.resize(s.size() - sizeof(z));
}

inline bool is_dot_or_dotdot(const wchar_t* p) {
    return p[0] == L'.' && (!p[1] || (p[1] == L'.' && !p[2]));
}

static bool _rmdir(fastring& s, wchar_t c) {
    const size_t n = s.size();
    pathcat(s, c, L"*");

    WIN32_FIND_DATAW e;
    HANDLE h = FindFirstFileW((PWC)s.data(), &e);
    if (h == INVALID_HANDLE_VALUE) {
        s.resize(n);
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }

    do {
        if (e.dwFileAttributes & g_attr_dir) {
            if (is_dot_or_dotdot(e.cFileName)) continue;
            s.resize(n);
            pathcat(s, c, e.cFileName);
            if (!_rmdir(s, c)) goto err;
        } else {
            s.resize(n);
            pathcat(s, c, e.cFileName);
            if (!DeleteFileW((PWC)s.data()) && GetLastError() != ERROR_FILE_NOT_FOUND) {
                goto err;
            }
        }
    } while (FindNextFileW(h, &e));

    FindClose(h);
    s.resize(n);
    *(wchar_t*)(s.data() + n) = L'\0';
    return RemoveDirectoryW((PWC)s.data());

  err:
    FindClose(h);
    return false;
}

bool remove(const char* path, bool r) {
    const wchar_t* wpath = widen(path);
    const DWORD attr = _getattr(wpath);
    if (attr == g_bad_attr) return true; // not exists
    if (!(attr & g_attr_dir)) return DeleteFileW(wpath);
    if (!r) return RemoveDirectoryW(wpath);

    const wchar_t c = strrchr(path, '/') ? L'/' : L'\\';
    return _rmdir(cache(), c);
}

bool mv(const char* from, const char* to) {
    PWC x, y;
    widen(from, to, &x, &y);

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
    fastring s(to);
    if (!s.ends_with(c)) s.append(c);
    s.append(p ? p + 1 : from);

    widen(from, s.c_str(), &x, &y);
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

bool rename(const char* from, const char* to) {
    PWC x, y;
    widen(from, to, &x, &y);
    return MoveFileW(x, y);
}

bool symlink(const char* dst, const char* lnk) {
    PWC x, y;
    widen(dst, lnk, &x, &y);
    const DWORD a = _getattr(y);
    if (a != g_bad_attr && (a & g_attr_lnk)) {
        (a & g_attr_dir) ? RemoveDirectoryW(y) : DeleteFileW(y);
    }
    const DWORD d = _isdir(x) ? 1 : 0;
    return CreateSymbolicLinkW(y, x, d);
}

#define nullfd INVALID_HANDLE_VALUE

namespace xx {
HANDLE open(const char* path, char mode) {
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

co::vector<fastring> dir::all() const {
    dctx* d = (dctx*)_p;
    if (!d || d->d == INVALID_HANDLE_VALUE) return co::vector<fastring>();

    co::vector<fastring> r(8);
    do {
        wchar_t* const p = d->e.cFileName;
        if (!is_dot_or_dotdot(p)) {
            r.push_back(narrow(p));
        }
    } while (FindNextFileW(d->d, &d->e));
    return r;
}

fastring dir::iterator::operator*() const {
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
