#ifdef _WIN32
#include "co/fs.h"
#include "co/alloc.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef _MSC_VER
#pragma warning (disable:4800)
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
    if (mode == 'r') {
        return CreateFileA(path, GENERIC_READ, 7, 0, OPEN_EXISTING, 0, 0);
    } else if (mode == 'a') {
        return CreateFileA(path, FILE_APPEND_DATA, 7, 0, OPEN_ALWAYS, 0, 0);
    } else if (mode == 'w') {
        return CreateFileA(path, GENERIC_WRITE, 7, 0, CREATE_ALWAYS, 0, 0);
    } else if (mode == 'm') {
        return CreateFileA(path, GENERIC_WRITE, 7, 0, OPEN_ALWAYS, 0, 0);
    } else {
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

} // namespace fs

#endif
