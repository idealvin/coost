#pragma once

#include "def.h"
#include "fastring.h"
#include "fastream.h"

namespace fs {

__codec bool exists(const char* path);

__codec bool isdir(const char* path);

// modify time
__codec int64 mtime(const char* path);

// file size
__codec int64 fsize(const char* path);

// p = false  ->  mkdir
// p = true   ->  mkdir -p
__codec bool mkdir(const char* path, bool p = false);

// rf = false  ->  rm or rmdir
// rf = true   ->  rm -rf
__codec bool remove(const char* path, bool rf = false);

__codec bool rename(const char* from, const char* to);

// administrator privileges required on windows
__codec bool symlink(const char* dst, const char* lnk);

inline bool exists(const fastring& path) {
    return fs::exists(path.c_str());
}

inline bool exists(const std::string& path) {
    return fs::exists(path.c_str());
}

inline bool isdir(const fastring& path) {
    return fs::isdir(path.c_str());
}

inline bool isdir(const std::string& path) {
    return fs::isdir(path.c_str());
}

inline int64 mtime(const fastring& path) {
    return fs::mtime(path.c_str());
}

inline int64 mtime(const std::string& path) {
    return fs::mtime(path.c_str());
}

inline int64 fsize(const fastring& path) {
    return fs::fsize(path.c_str());
}

inline int64 fsize(const std::string& path) {
    return fs::fsize(path.c_str());
}

inline bool mkdir(const fastring& path, bool p=false) {
    return fs::mkdir(path.c_str(), p);
}

inline bool mkdir(const std::string& path, bool p=false) {
    return fs::mkdir(path.c_str(), p);
}

inline bool remove(const fastring& path, bool rf=false) {
    return fs::remove(path.c_str(), rf);
}

inline bool remove(const std::string& path, bool rf=false) {
    return fs::remove(path.c_str(), rf);
}

inline bool rename(const fastring& from, const fastring& to) {
    return fs::rename(from.c_str(), to.c_str());
}

inline bool rename(const std::string& from, const std::string& to) {
    return fs::rename(from.c_str(), to.c_str());
}

inline bool symlink(const fastring& dst, const fastring& lnk) {
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
class __codec file {
  public:
    static const int seek_beg = 0;
    static const int seek_cur = 1;
    static const int seek_end = 2;

    file() : _p(0) {}
    ~file();

    file(const char* path, char mode) : _p(0) {
        this->open(path, mode);
    }

    file(const fastring& path, char mode)    : file(path.c_str(), mode) {}
    file(const std::string& path, char mode) : file(path.c_str(), mode) {}

    file(file&& f) { _p = f._p; f._p = 0; }

    file(const file& x) = delete;
    void operator=(const file& x) = delete;
    void operator=(file&& x) = delete;

    explicit operator bool() const;
    
    bool operator!() const {
        return !(bool)(*this);
    }

    const fastring& path() const;

    int64 size()  const { return fs::fsize (this->path()); }
    bool exists() const { return fs::exists(this->path()); }

    bool open(const char* path, char mode);

    bool open(const fastring& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    bool open(const std::string& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    void close();

    void seek(int64 off, int whence=seek_beg);

    size_t read(void* buf, size_t n);

    fastring read(size_t n);

    size_t write(const void* s, size_t n);

    size_t write(const char* s) {
        return this->write(s, strlen(s));
    }

    size_t write(const fastring& s) {
        return this->write(s.data(), s.size());
    }

    size_t write(const std::string& s) {
        return this->write(s.data(), s.size());
    }

    size_t write(char c) {
        return this->write(&c, 1);
    }

  private:
    void* _p;
};

// open mode:
//   'a': append       created if not exists
//   'w': write        created if not exists, truncated if exists
class __codec fstream {
  public:
    fstream() : _s(8192) {}
    explicit fstream(size_t cap) : _s(cap) {}

    fstream(const char* path, char mode, size_t cap=8192)
        : _s(cap), _f(path, mode == 'w' ? 'w' : 'a') {
    }

    fstream(const fastring& path, char mode, size_t cap=8192)
        : fstream(path.c_str(), mode, cap) {
    }

    fstream(const std::string& path, char mode, size_t cap=8192)
        : fstream(path.c_str(), mode, cap) {
    }

    fstream(fstream&& fs)
        : _s(std::move(fs._s)), _f(std::move(fs._f)) {
    }

    ~fstream() {
        this->close();
    }

    explicit operator bool() const {
        return (bool)_f;
    }

    bool operator!() const {
        return !(bool)_f;
    }

    bool open(const char* path, char mode) {
        this->close();
        return _f.open(path, mode == 'w' ? 'w' : 'a');
    }

    bool open(const fastring& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    bool open(const std::string& path, char mode) {
        return this->open(path.c_str(), mode);
    }

    void reserve(size_t n) { _s.reserve(n); }

    void flush() {
        if (!_s.empty()) {
            _f.write(_s.data(), _s.size());
            _s.clear();
        }
    }

    void close() {
        this->flush();
        _f.close();
    }

    // n <= cap - szie         ->   append
    // cap - size < n <= cap   ->   flush and append
    // n > cap                 ->   flush and write
    fstream& append(const void* s, size_t n) {
        if (_s.capacity() < _s.size() + n) this->flush();
        n <= _s.capacity() ? ((void)_s.append(s, n)) : ((void)_f.write(s, n));
        return *this;
    }

    fstream& operator<<(const char* s) {
        return this->append(s, strlen(s));
    }

    fstream& operator<<(const fastring& s) {
        return this->append(s.data(), s.size());
    }

    fstream& operator<<(const std::string& s) {
        return this->append(s.data(), s.size());
    }

    fstream& operator<<(const fastream& s) {
        return this->append(s.data(), s.size());
    }

    template<typename T>
    fstream& operator<<(T v) {
        if (_s.capacity() < _s.size() + 24) this->flush();
        _s << v;
        return *this;
    }

  private:
    fastream _s;
    fs::file _f;

    DISALLOW_COPY_AND_ASSIGN(fstream);
};

} // fs
