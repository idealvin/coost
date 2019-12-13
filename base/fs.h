#pragma once

#include "def.h"
#include "fastring.h"
#include "fastream.h"

namespace fs {

bool exists(const char* path);

bool isdir(const char* path);

// modify time
int64 mtime(const char* path);

// file size
int64 fsize(const char* path);

// p = false  ->  mkdir
// p = true   ->  mkdir -p
bool mkdir(const char* path, bool p=false);

// rf = false  ->  rm or rmdir
// rf = true   ->  rm -rf
bool remove(const char* path, bool rf=false);

bool rename(const char* from, const char* to);

// administrator privileges required on windows
bool symlink(const char* dst, const char* lnk);

template<typename S>
inline bool exists(const S& path) {
    return fs::exists(path.c_str());
}

template<typename S>
inline bool isdir(const S& path) {
    return fs::isdir(path.c_str());
}

template<typename S>
inline int64 mtime(const S& path) {
    return fs::mtime(path.c_str());
}

template<typename S>
inline int64 fsize(const S& path) {
    return fs::fsize(path.c_str());
}

template<typename S>
inline bool mkdir(const S& path, bool p=false) {
    return fs::mkdir(path.c_str(), p);
}

template<typename S>
inline bool remove(const S& path, bool rf=false) {
    return fs::remove(path.c_str(), rf);
}

template<typename S>
inline bool rename(const S& from, const S& to) {
    return fs::rename(from.c_str(), to.c_str());
}

template<typename S>
inline bool symlink(const S& dst, const S& lnk) {
    return fs::symlink(dst.c_str(), lnk.c_str());
}

// open mode:
//   'r': read         open if exists
//   'a': append       created if not exists
//   'w': write        created if not exists, truncated if exists
//   'm': modify       like 'w', but not truncated if exists
class file {
  public:
    static const int seek_beg = 0;
    static const int seek_cur = 1;
    static const int seek_end = 2;

    file() : _p(0) { }

    file(const char* path, char mode) : _p(0) {
        this->open(path, mode);
    }

    file(file&& f) {
        _p = f._p;
        f._p = 0;
    }

    ~file();

    file(const file& x) = delete;
    void operator=(const file& x) = delete;
    void operator=(file&& x) = delete;

    operator bool() const;

    const fastring& path() const;

    int64 size() const {
        return fs::fsize(this->path());
    }

    bool exists() const {
        return fs::exists(this->path());
    }

    bool open(const char* path, char mode);

    void close();

    void seek(int64 off, int whence=seek_beg);

    size_t read(void* buf, size_t size);

    fastring read(size_t size);

    size_t write(const void* buf, size_t size);

    size_t write(const char* s) {
        return this->write(s, strlen(s));
    }

    template<typename S>
    size_t write(const S& s) {
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
class fstream {
  public:
    explicit fstream(size_t cap=8192)
        : _s(cap) {
    }

    explicit fstream(const char* path, char mode, size_t cap=8192)
        : _s(cap), _f(path, mode == 'w' ? 'w' : 'a') {
    }

    fstream(fstream&& fs)
        : _s(std::move(fs._s)), _f(std::move(fs._f)) {
    }

    ~fstream() {
        this->close();
    }

    operator bool() const {
        return _f;
    }

    bool open(const char* path, char mode) {
        return _f.open(path, mode == 'w' ? 'w' : 'a');
    }

    void flush() {
        if (!_s.empty()) {
            _f.write(_s.data(), _s.size());
            _s.clear();
        }
    }

    void close() {
        if (_f) {
            this->flush();
            _f.close();
        }
    }

    // n <= cap - szie         ->   append
    // cap - size < n <= cap   ->   flush and append
    // n > cap                 ->   flush and write
    fstream& append(const void* s, size_t n) {
        if (_s.capacity() < _s.size() + n) this->flush();
        n <= _s.capacity() ? ((void) _s.append(s, n)) : ((void) _f.write(s, n));
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
    fstream& operator<<(const T& v) {
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
