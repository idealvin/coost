#pragma once

#include "error.h"
#include "string.h"

namespace fs {

bool exists(const char* path);

bool isdir(const char* path);

// modify time
int64 mtime(const char* path);

// file size
int64 fsize(const char* path);

// p = false  ->  mkdir
// p = true   ->  mkdir -p
bool mkdir(const char* path, bool p = false);

// async-signal-safe version
bool mkdir(char* path, bool p);

// r = false  ->  rm or rmdir
// r = true   ->  rm -r
bool rm(const char* path, bool r = false);

// rename or move a file or directory
bool mv(const char* from, const char* to);

bool symlink(const char* dst, const char* lnk);

inline bool exists(const co::string& path) {
    return fs::exists(path.c_str());
}

inline bool exists(const std::string& path) {
    return fs::exists(path.c_str());
}

inline bool isdir(const co::string& path) {
    return fs::isdir(path.c_str());
}

inline bool isdir(const std::string& path) {
    return fs::isdir(path.c_str());
}

inline int64 mtime(const co::string& path) {
    return fs::mtime(path.c_str());
}

inline int64 mtime(const std::string& path) {
    return fs::mtime(path.c_str());
}

inline int64 fsize(const co::string& path) {
    return fs::fsize(path.c_str());
}

inline int64 fsize(const std::string& path) {
    return fs::fsize(path.c_str());
}

inline bool mkdir(const co::string& path, bool p=false) {
    return fs::mkdir(path.c_str(), p);
}

inline bool mkdir(const std::string& path, bool p=false) {
    return fs::mkdir(path.c_str(), p);
}

inline bool rm(const co::string& path, bool r=false) {
    return fs::rm(path.c_str(), r);
}

inline bool rm(const std::string& path, bool r=false) {
    return fs::rm(path.c_str(), r);
}

inline bool mv(const co::string& from, const co::string& to) {
    return fs::mv(from.c_str(), to.c_str());
}

inline bool mv(const std::string& from, const std::string& to) {
    return fs::mv(from.c_str(), to.c_str());
}

inline bool symlink(const co::string& dst, const co::string& lnk) {
    return fs::symlink(dst.c_str(), lnk.c_str());
}

inline bool symlink(const std::string& dst, const std::string& lnk) {
    return fs::symlink(dst.c_str(), lnk.c_str());
}

// open mode:
//   'r': read         open if exists
//   'a': append       created if not exists
//   'w': write        created if not exists, truncated if exists
//   'm': modify       like 'w', but not truncated if exists
//   '+': read/write   created if not exists
struct file {
    enum _seekfrom_t {
        seek_beg = 0,
        seek_cur = 1,
        seek_end = 2,
    };

    constexpr file() noexcept : _p(nullptr) {}
    ~file();

    // @n: reserve n bytes of memory for the path
    explicit file(size_t n);

    file(const char* path, char mode) : _p(nullptr) {
        this->open(path, mode);
    }

    file(const co::string& path, char mode)
        : file(path.c_str(), mode) {
    }

    file(const std::string& path, char mode)
        : file(path.c_str(), mode) {
    }

    file(file&& f) noexcept : _p(f._p) {
        f._p = 0;
    }

    file(const file& x) = delete;
    void operator=(const file& x) = delete;
    void operator=(file&& x) = delete;

    explicit operator bool() const noexcept;
    
    bool operator!() const noexcept {
        return !(bool)(*this);
    }

    const char* path() const noexcept;

    int64 size()  const noexcept { return fs::fsize (this->path()); }
    bool exists() const noexcept { return fs::exists(this->path()); }

    bool open(const char* path, char mode);

    bool open(const co::string& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    bool open(const std::string& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    void close();

    bool seek(int64 off, _seekfrom_t whence=seek_beg);

    size_t read(void* buf, size_t n);

    co::string read(size_t n);

    size_t write(const void* s, size_t n);

    size_t write(const char* s) {
        return this->write(s, strlen(s));
    }

    size_t write(const co::string& s) {
        return this->write(s.data(), s.size());
    }

    size_t write(const std::string& s) {
        return this->write(s.data(), s.size());
    }

    size_t write(char c) {
        return this->write(&c, 1);
    }

    void* _p;
};

struct dir {
    constexpr dir() noexcept : _p(nullptr) {}
    ~dir();

    explicit dir(const char* path) : _p(nullptr) {
        this->open(path);
    }

    explicit dir(const co::string& path)  : dir(path.c_str()) {}
    explicit dir(const std::string& path) : dir(path.c_str()) {}

    dir(dir&& d) noexcept : _p(d._p) { d._p = 0; }

    dir(const dir&) = delete;
    void operator=(const dir&) = delete;
    void operator=(dir&&) = delete;

    bool open(const char* path);
    bool open(const co::string& path)  { return this->open(path.c_str()); }
    bool open(const std::string& path) { return this->open(path.c_str()); }
    void close();

    const char* path() const noexcept;

    // return all entries
    co::vector<co::string> all() const;

    struct iterator {
        explicit iterator(void* p) noexcept : _p(p) {}
        ~iterator() = default;

        co::string operator*() const;
        iterator& operator++(); // ++it

        bool operator==(const iterator& it) const noexcept {
            return _p == it._p;
        }

        bool operator!=(const iterator& it) const noexcept {
            return !this->operator==(it);
        }

        void* _p;
    };

    iterator begin() const;
    iterator end() const { return iterator(nullptr); }

    void* _p;
};

} // fs
