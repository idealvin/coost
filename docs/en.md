# Documents for co v1.1

<font face="Arial" size=3>
<center>
Alvin &nbsp;2020/07/07
</center>
<center>
idealvin@qq.com
</center>
</font>

Section 1-10 was translated by [Leedehai](https://github.com/Leedehai), 11-15 was translated by [daidai21](https://github.com/daidai21). Thanks here.

[CO](https://github.com/idealvin/co/) is an elegant and efficient C ++ basic library that supports Linux, Windows and Mac platforms. This document will introduce the components of CO and their usages.


## 1. Overview

`CO` pursues minimalism and efficiency. It does not rely on third-party libraries such as [boost](https://www.boost.org/), and uses only a few C++11 features.

- CO contains the following functional components:
    - Basic definitions (def)
    - Atomic operations (atomic)
    - Fast random number generator (random)
    - LruMap
    - Fast string casting for basic types (fast)
    - Efficient byte stream (fastream)
    - Efficient strings (fastring)
    - String operations (str)
    - Command line arguments and configuration file parsing library (flag)
    - Efficient streaming log library (log)
    - Unit testing framework (unitest)
    - Efficient json library (json)
    - Time library (time)
    - Thread library (thread)
    - Coroutine library (co)
    - Network library (so)
    - Hash library
    - Path library
    - File system operations (fs)
    - System operations (os)


## 2. Basic definitions (def)

include: [co/def.h](https://github.com/idealvin/co/blob/master/include/co/def.h).

### 2.1 Fixed-width integers

```cpp
 int8   int16   int32   int64
uint8  uint16  uint32  uint64
```

Their widths don't vary across platforms, so there should be no compatibility issues. [Google Code Style](https://google.github.io/styleguide/cppguide.html#Integer_Types) suggests that one should not use built-in integer types like `short`, `long`, `long long`, except for `int`.

`def.h` also provides the minimum and maximum values of the aforementioned integer types:

```cpp
MAX_UINT8  MAX_UINT16  MAX_UINT32  MAX_UINT64
MAX_INT8   MAX_INT16   MAX_INT32   MAX_INT64
MIN_INT8   MIN_INT16   MIN_INT32   MIN_INT64
```

### 2.2 Read/write 1, 2, 4, 8 bytes

`def.h` provides the below macros to facilitate reading/writing 1/2/4/8 bytes of data (note memory alignment is important):

```cpp
load8  load16  load32  load64
save8  save16  save32  save64
```

- Examples

```cpp
uint64 v;                  // 8 bytes
save32(&v, 7);             // set the first 4 bytes to 7
uint16 x = load16(&v);     // read the first 2 bytes of v
```

### 2.3 DISALLOW_COPY_AND_ASSIGN

This macro deletes copy constructors and assignment operators in a C++ class.

- Example

```cpp
class T {
  public:
    T();
    DISALLOW_COPY_AND_ASSIGN(T);
};
```

### 2.4 force_cast type coercion

`force_cast` is an encapsulation of C-style type casting

- Example

```cpp
char c = force_cast<char>(97); // char c = (char) 97;
```

### 2.5 __forceinline and __thread

[__forceinline](https://docs.microsoft.com/en-us/cpp/cpp/inline-functions-cpp?view=vs-2019#inline-__inline-and-__forceinline) is a keyword in Visual Studio on Windows. On other platforms, it can be provided by the macro below:

```cpp
#define __forceinline __attribute__((always_inline))
```

[__thread](https://gcc.gnu.org/onlinedocs/gcc-4.7.4/gcc/Thread-Local.html) is a keyword in GCC to support [TLS](https://wiki.osdev.org/Thread_Local_Storage). On Windows, it can be provided by the macro below:

```cpp
#define __thread __declspec(thread)
```

- Example

```cpp
// get the ID of the current thread
__forceinline unsigned int gettid() {
    static __thread unsigned int id = 0;
    if (id != 0) return id;
    return id = __gettid();
}
```

### 2.6 unlikely

The macro `unlikely` is used to aid branch predictions in GCC and Clang:

```cpp
// it is logically equivalent to (v == -1), but provides a hint to the compiler 
// that the probability of v == -1 is small
if (unlikey(v == -1)) {
    cout << "v == -1" << endl;
}
```


## 3. Atomic operations

include: [co/atomic.h](https://github.com/idealvin/co/tree/master/include/co/atomic.h).

Library `atomic` implements the following atomic operations:

```cpp
atomic_inc        atomic_dec        atomic_add        atomic_sub
atomic_fetch_inc  atomic_fetch_dec  atomic_fetch_add  atomic_fetch_sub

atomic_or         atomic_and        atomic_xor
atomic_fetch_or   atomic_fetch_and  atomic_fetch_xor

atomic_swap    atomic_compare_swap
atomic_get     atomic_set    atomic_reset
```

These operations is suitable for 1-, 2-, 4- and 8-byte data. The `fetch` versions of `inc`, `dec`, `add`, `sub`, `or`, `and`, `xor` are different from the vanilla versions, in that the former returns the value before the operations while the latter returns the value after the operations.

- Examples

```cpp
bool b = false;
int i = 0;
uint64 u = 0;
void* p = 0;

atomic_inc(&i);                 // return ++i;
atomic_dec(&i);                 // return --i;
atomic_add(&i, 3);              // return i += 3;
atomic_sub(&i, 3);              // return i -= 3;
atomic_fetch_inc(&u);           // return u++;

atomic_or(&i, 8);               // return i |= 8;
atomic_and(&i, 7);              // return i &= 7;
atomic_xor(&i, 7);              // return i ^= 7;
atomic_fetch_xor(&i, 7);        // v = i; i ^= 7; return v;

atomic_swap(&b, true);          // v = b; b = true; return v;
atomic_compare_swap(&i, 0, 1);  // v = i; if (i == 0) i = 1; return v;

atomic_get(&u);                 // return u;
atomic_set(&u, 7);              // u = 7;
atomic_reset(&i);               // i = 0;

// atomic operations on pointers
atomic_set(&p, 0);
atomic_swap(&p, 8);
atomic_compare_swap(&p, 0, 8);
```


## 4. Random number generator (random)

include: [co/random.h](https://github.com/idealvin/co/blob/master/include/co/random.h).

`Random` is a speedy pseudo-random number generator, capable of generating random integers between 1 and 2G-2 without consecutive repetitions. Though [leveldb](https://github.com/google/leveldb/blob/master/util/random.h) uses this algorithm, this library chose a different constant `16385`, which is faster.

- Example

```cpp
Random r(7);      // 7 is the seed, which defaults to 1
int n = r.next(); // !! not thread-safe
```


## 5. LruMap

include: [co/lru_map.h](https://github.com/idealvin/co/blob/master/include/co/lru_map.h).

LRU is a popular caching strategy. When its size grows to the capacity, it will purge the least recently used data. `LruMap` is based on the implementation of `std::list` and `std::unordered_map`. Its elements are stored without order.

- Example

```cpp
LruMap<int, int> m(128);         // capacity: 128
m.insert(1, 23);                 // when m.size() > 128, delete the last element
                                 // in the internal list (least recently used).
                                 // !! if key already exists, no inseration done
auto it = m.find(1);             // if found, put 1 at the beginning of the list
if (it != m.end()) m.erase(it);  // erase by iterator
m.erase(it->first);              // erase by key
```


## 6. Fast string casting for basic types (fast)

include: [co/fast.h](https://github.com/idealvin/co/blob/master/include/co/fast.h).

Library `fast` provides functions below:

```cpp
u32toh  u64toh  u32toa  u64toa  i32toa  i64toa  dtoa
```

`xtoh`-family converts an integer into a hexadecimal string. It caches the results corresponding to the first 256 numbers (2 bytes) in a table. Testing across platforms suggested it is 10-25 times faster than `snprintf`.

`xtoa`-family converts an integer into an ASCII decimal string. It caches the results corresponding to the first 10000 numbers (4-bytes). Testing across platforms suggested it is 10-25 times faster than `snprintf`.

`dtoa` uses the implementation by [Milo Yip] (https://github.com/miloyip), see [miloyip/dtoa-benchmark] (https://github.com/miloyip/dtoa-benchmark) for details. The early implementation based on `LruMap` was deprecated.

- Examples

```cpp
char buf[32];
int len = fast::u32toh(255, buf); // "0xff", len is 4
int len = fast::i32toa(-99, buf); // "-99", len is 3
int len = fast::dtoa(0.123, buf); // "0.123", len is 5
```


## 7. Efficient byte stream

include: [co/fastream.h](https://github.com/idealvin/co/blob/master/include/co/fastream.h).

`std::ostringstream` in the C++ standard library is significantly less performant than `snprintf`. `fastream` inherits from the `fast::stream` class and supports streaming output and binary append operations. Among them, streaming output is about 10~30 times faster than snprintf on different platforms.

- Examples

```cpp
fastream fs(1024);          // preallocate 1K memory
fs << "hello world" << 23;  // stream mode

int i = 23;
char buf[8];
fs.append(buf, 8);      // append 8 bytes
fs.append(&i, 4);       // append 4 bytes
fs.append(i);           // append 8 bytes, same as fs.append(&i, 4)
fs.append((int16) 23);  // append 2 bytes
fs.append('c');         // append a single character
fs.append(100, 'c');    // append 100 character 'c'
fs.append('c', 100);    // append 100 character 'c'

fs.c_str();             // return a C-style string
fs.str();               // return a C++ string with deep-copy
fs.data();              // return pointer to data
fs.size();              // return data length
fs.capacity();          // capacity

fs.reserve(4096);       // reserve at least 4K memory
fs.resize(32);          // size -> 32, content in buffer is unchanged
fs.clear();             // size -> 0
fs.swap(fastream());    // swap
```

- Precautions

For performance reasons, no security check is performed during the `append` operation for `fastream`. The following code is not safe:

```cpp
fastream f;
f.append("hello");
f.append(f.c_str() + 1); // unsafe, internal memory overlap is not considered
```


## 8. Efficient strings (fastring)

include: [co/fastring.h](https://github.com/idealvin/co/blob/master/include/co/fastring.h).

`fastring` is a string type in co. Like `fastream`, it inherits from `fast::stream`, so in addition to basic string operations, it also supports streaming output operations:
```cpp
fastring s;
s << "hello world " << 1234567;
```

In the early implementation of fastring, reference counting was used, which made the copying behavior of fastring different from `std::string`, which caused confusion. To better replace std::string, reference counting has been removed from the refactored version.

Fastring supports almost all operations of fastream, but one thing is different from fastream, fastring will perform security check when `append`:

```cpp
fastring s("hello");
fastream f;
f.append("hello");

s.append(s.c_str() + 1); // safe, memory overlap detected, special processing
f.append(f.c_str() + 1); // unsafe, does not check memory overlap
```

- Examples

```cpp
fastring s;                // empty string, without memory allocation
fastring s(32);            // empty string, with memory preallocated (32 bytes)
fastring s("hello");       // non-empty strings
fastring s(88, 'x');       // initialize s to be 88 'x' characters
fastring s('x', 88);       // initialize s to be 88 'x' characters
fastring t = s;            // create a new string through memory copy

s << "hello " << 23;       // streaming output
s += "xx";                 // append
s.append("xx");            // append  <==>  s += "xx";
s.swap(fastring());        // swap

s + "xxx";                 // +
s > "xxx";                 // >
s < "zzz"                  // <
s <= "zz"                  // <=
s >= "zz"                  // >=

s.find('c');               // character lookup
s.find("xx", 3);           // substring lookup starting from index 3
s.rfind('c');              // character reverse-lookup
s.rfind("xx");             // substring reverse-lookup
s.find_first_of("xy");     // find the first occurence of a character in "xy"
s.find_first_not_of("xy"); // find the first occurence of a character not in "xy"
s.find_last_of("xy");      // find the last occurence of a character in "xy"
s.find_last_not_of("xy");  // find the last occurence of a character not in "xy"
s.starts_with('x');        // check whether s starts with 'x'
s.starts_with("xx");       // check whether s starts with "xx"
s.ends_with('x');          // check whether s ends with 'x'
s.ends_with("xx");         // check whether s ends with "xx"

s.replace("xxx", "yy");    // replace "xxx" in s with "yy"
s.replace("xxx", "yy", 3); // replace "xxx" in s with "yy" at most 3 times

s.strip();                 // strips whitespaces " \t\r\n" from both ends of s
s.strip("ab");             // strips 'a', 'b' from both ends of s
s.strip("ab", 'l');        // strips 'a', 'b' from the left endsof s
s.strip("ab", 'r');        // strips 'a', 'b' from the right endsof s

s.tolower();               // convert characters in s to lowercases
s.toupper();               // convert characters in s to uppercases
s.lower();                 // return lowercase of s, without mutating s
s.upper();                 // return uppercase of s, without mutating s
s.match("x*y?z");          // glob pattern matching
                           // '*' for any string, '?' for a single character
```

- Precautions

When fastring contains binary characters, do not use the `find` series of operations:

- find()
- rfind()
- find_first_of()
- find_first_not_of()
- find_last_of()
- find_last_not_of()

The above methods are based on `strrchr`, `strstr`, `strcspn`, `strspn`, etc. When the string contains binary characters, there is no guarantee that the correct result will be obtained.


## 9. String operations

include: [co/str.h](https://github.com/idealvin/co/blob/master/include/co/str.h).

### 9.1 Splitting

`split` splits a string into several pieces.

- Prototype

```cpp
// @s: the original string: fastring or const char*
// @c: delimiter: a single character or a '\0'-terminated string
// @n: allowed times of splitting: 0/-1 means no limit (default is 0)
std::vector<fastring> split(s, c, n=0);
```

- Examples

```cpp
str::split("x y z", ' ');     // ->  [ "x", "y", "z" ]
str::split("|x|y|", '|');     // ->  [ "", "x", "y" ]
str::split("xooy", "oo");     // ->  [ "x", "y"]
str::split("xooy", 'o');      // ->  [ "x", "", "y" ]
str::split("xooy", 'o', 1);   // ->  [ "x", "oy" ]
```

### 9.2 Stripping

`strip` strips away certain character on one or both ends of a string, and returns the new string (the original is intact).

- Prototype

```cpp
// @s: the original string: fastring or const char*
// @c: the set of characters to be stripped: a single character or a string
// @d: which end: 'l'/'L' means the left end, 'r'/'R' means the right ends,
//                'b' (the default) means both ends
fastring strip(s, c=" \t\r\n", d='b');
```

- Examples

```cpp
str::strip("abxxa", "ab");       // -> "xx"    strip both ends
str::strip("abxxa", "ab", 'l');  // -> "xxa"   strip the left end
str::strip("abxxa", "ab", 'r');  // -> "abxx"  strip the right end
```

### 9.3 Replacing

`replace` replaces substrings of the original, and returns the new string (the original is intact).

- Prototype

```cpp
// @s:    the original string: fastring or const char*
// @sub:  old substring to be replaced
// @to:   new substring to be replaced with
// @n:    allowed times of replacing: 0/-1 means no limit (default is 0)
fastring replace(s, sub, to, n=0);
```

- Examples

```cpp
str::replace("xooxoox", "oo", "ee");     // -> "xeexeex"
str::replace("xooxoox", "oo", "ee", 1);  // -> "xeexoox"
```

### 9.4 String to built-in types

The library provides the following strings to convert a string into primitive types:

```cpp
to_int32  to_int64  to_uint32  to_uint64  to_bool  to_double
```

- Notes
    - if conversion errs, throw an exception of `const char*` type
    - when converting to integers, the parameter could end with unit `k`, `m`, `g`, `t`, `p` (case-insensitive).

- Examples

```cpp
bool x = str::to_bool("false");    // "true" or "1" -> true
                                   // "false" or "0" -> false
double x = str::to_double("3.14"); // 3.14

int32 x = str::to_int32("-23");    // -23
int64 x = str::to_int64("4k");     // 4096
uint32 x = str::to_uint32("8M");   // 8 << 20
uint64 x = str::to_uint64("8T");   // 8ULL << 40
```

### 9.5 Built-in types to string

The library provides function `from` to convert a built-in type to string.

- Examples:

```cpp
fastring s = str::from(true);  // -> "true"
fastring s = str::from(23);    // -> "23"
fastring s = str::from(3.14);  // -> "3.14"
```

### 9.6 debug string

The library provides function `dbg` to convert an object to a string for debugging.

- Prototype

```cpp
// @v: built-in types, sttings, or common STL containers (vector, map, set)
fastring dbg<T>(v);
```

- Example

```cpp
std::vector<int> v { 1, 2, 3 };
std::set<int> s { 1, 2, 3 };
std::map<int, int> m { {1, 1}, {2, 2} };
str::dbg(v);    // -> "[1,2,3]"
str::dbg(s);    // -> "{1,2,3}"
str::dbg(m);    // -> "{1:1,2:2}

str::dbg(true); // -> "true"
str::dbg(23);   // -> "23"
str::dbg("23"); // -> "\"23\"", string, with quotes added
```

- If the parameter string contains `"`, the returned string from `dbg()` may look weird. But it is fine in most cases since the function is used to print logs.


## 10. Commandline arguments and config file parsing (flag)

include: [co/flag.h](https://github.com/idealvin/co/blob/master/include/co/flag.h).

### 10.1 Concepts

Library `flag` is a commandline arguments and configuration file parser like [google gflags](https://github.com/gflags/gflags). The code defines static global variables, and parses commandline arguments and configuration file at runtime, and modifies the value of these variables accordingly.

#### 10.1.1 flag variables

**flag variables** refer to the static global variables defined by macros in this library. For example, the code snippet below defines a flag variable named `x`, and its corresponding global variable is named `FLG_x`.
```cpp
DEF_bool(x, false, "xxx"); // bool FLG_x = false;
```

The library supports 7 types of flag variables:
```cpp
bool, int32, int64, uint32, uint64, double, string
```

#### 10.1.2 command line flags

Strings in commandline arguments often take the form of `-x=y`. Here, `x` is a **commandline flag** (**flag** below). Flags in the commandline and flag variables in the code are one-to-one mapped to each other. The library is flexible for ease of using:

- the leading `-` can be omitted from `-x=y`, so you can write `x=y`.
- `-x=y` can be also writen as `-x y`.
- the number of `-` before `x=y` is unlimited.
- for boolean flags, `-b=true` can be abbreviated as `-b` (the leading `-` must be kept when using this abbreviation).

```sh
./exe -b -i=32 s=hello xx  # b,i,s are flags, but xx is not
```

### 10.2 initialization of flag library

The library provides only one API `flag::init()`. It is used to initialize the library and parses commandline arguments and config files.

```cpp
// Workflow
// 1. Scan commandline arguments, and divide it into flag and non-flag.
// 2. Update the value of FLG_config according to the flag arguments. 
//    If it is not empty, parse the given config file.
// 3. Update the values of other flag variables based on the flag arguments.
// 4. If FLG_mkconf is not empty, generate a config file and exit.
// 5. if FLG_daemon is true, run the program in the background.

// If an error is encountered while parsing, output an error message and exit.
// If no error is encountered, return a list of non-flag arguments.
std::vector<fastring> init(int argc, char** argv);
```

This function needs to be called upon entering `main()`:

```cpp
#include "co/flag.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
}
```

### 10.3 Define, declare, and use flag variables

#### 10.3.1 Define flag variables

This library implements 7 macros to define 7 different flag variable types:
```cpp
DEF_bool  DEF_int32  DEF_int64  DEF_uint32  DEF_uint64  DEF_double  DEF_string
```

The snippet below defines two types of flag variables; one is boolean and the other is string:

```cpp
DEF_bool(b, false, "comments");  // bool FLG_b = false;
DEF_string(s, "x", "comments");  // fastring FLG_s = "x";
```

Macro `DEF_xxx` has three parameter: the first is the flag variable name, the second is the default value, and the third is the comments. Note that:
- the flag variable is global, and thus they shouldn't be defined in a header,
- the name of the flag variable should be unique.

#### 10.3.2 Declare flag variables

This library also implements 7 macros to define 7 different flag variable types:
```cpp
DEC_bool  DEC_int32  DEC_int64  DEC_uint32  DEC_uint64  DEC_double  DEC_string
```

The snippet below declares a variable of `int32`:
```cpp
DEC_int32(i32); // extern int32 FLG_i32;
```

Macro `DEC_xxx` has only one parameter, which is the flag variable name. Each variable can only be defined once, but can be declared multiple times. Declarations are generally used to refer to flag variables defined elsewhere.

#### 10.3.3 Use flag variables

After being defined or declared, the flag variables can be used like ordinary variables:

```cpp
DEC_bool(b);
if (!FLG_b) std::cout << "b is false" << std::endl;   

DEF_string(s, "hello", "xxx");
FLG_s += " world";
std::cout << FLG_s << std::endl;
```

### 10.4 Use flags in commandline

#### 10.4.1 Set flag variables' values in commandline:

Assume the program defines the following flags:
```cpp
DEF_bool(x, false, "bool x");
DEF_bool(y, false, "bool y");
DEF_int32(i, -32, "int32");
DEF_uint64(u, 64, "uint64");
DEF_double(d, 3.14, "double");
DEF_string(s, "hello world", "string");
```

The flags' values can be modified in commandline:

* `-x=y` can be also writen as `-x y` or `x=y`
  ```sh
  ./xx -i=8 u=88 -s="hello world"
  ./xx -i 8 -u 88 -s "hello world"
  ```

* For boolean flags, the `true` value can be omitted
  ```sh
  ./xx -x     # -x=true
  ```

* When multiple boolean flags are set to `true` and their names are single characters, they can be combined
  ```sh
  ./xx -xy    # -x=true -y=true
  ```

* Integer flags can have a trailing unit symbol `k`, `m`, `g`, `t`, or `p` (case-insensitive).
  ```sh
  ./xx i=-4k  # i=-4096
  ```

* Integer flags' values can also be octal or hexadecimal
  ```sh
  ./xx i=032  # i=26    octal
  ./xx u=0xff # u=255   hexadecimal
  ```

#### 10.4.2 Help messages

```sh
$ ./xx --help
usage:
    ./xx --                   print flags info
    ./xx --help               print this help info
    ./xx --mkconf             generate config file
    ./xx --daemon             run as a daemon (Linux)
    ./xx xx.conf              run with config file
    ./xx config=xx.conf       run with config file
    ./xx -x -i=8k -s=ok       run with commandline flags
    ./xx -x -i 8k -s ok       run with commandline flags
    ./xx x=true i=8192 s=ok   run with commandline flags
```

#### 10.4.3 Flag variable list

```sh
$ ./xx --
--config: .path of config file
	 type: string	     default: ""
	 from: ../../base/flag.cc
--mkconf: .generate config file
	 type: bool	     default: false
	 from: ../../base/flag.cc
```

### 10.5 Specify a config file for program

A configuration file can be specified for a program with flag `config`:
```sh
./xx config=xx.conf
./xx xx.conf    # if the config file name ends with '.conf' or 'config' and
                # it's the first non-flag parameter, config= can be omitted 
./xx -x xx.conf # -x is a flag, so xx.conf is the first non-flag parameter
```

Also, the config file can be specified by modifying the value of `FLG_config` before invoking `flag::init()`.

### 10.6 Auto-generate config files

A config file can be auto-generated by using `--mkconf`:
```sh
./xx --mkconf          # generate xx.conf in the same directory of xx
./xx --mkconf -x u=88  # replace default values with specified values
```

Configuration items are sorted by level, file name, and line number of code. When defining the flag, you can add `#n` at the beginning of the comments to specify the level. `n` must be an integer between 0 and 99. If not specified, it defaults to 10.

```cpp
// Specify the level as 0 (the lower level ranks first)
DEF_bool (daemon, false, "# 0 run program as a daemon");
```

- Special notes:
    - Flags whose comments start with `.` are **hidden** and will not be present in the generated config file, but `./xx --` still displays them.
    - Flag whose comments are empty are **stealth**, which means they will not be present in the generated config file, and won't be displayed by `./xx --`.

### 10.7 Format of config files

THe library allows a flexible format for config files:

- Ignore whitespaces before or at the end of lines, so manually writing them is freer and less error-prone.
- `#` and `//` stands for comments. Block comments and trailing comments are supported.
- each line has at most one flag, in the form of `x = y` for clarity.
- One or more whitespaces can be inserted before or after `=`.
- Lines can be concatenated with `\` at the end of the previous line.
- No escaping is supported to prevent ambiguity.

An example:
```sh
   # config file: xx.conf
     daemon = false            # background program (defined in co/flag)
     boo = true                # cannot be simplified as '-boo'

     s =                       # empty string
     s = hello \
         world                 # s = "helloworld"
     s = "http://github.com"   # '#' and '//' are not comments in a quoted string
     s = "I'm ok"              # string, use double quotes on both ends
     s = 'how are "U"'         # string, use single quotes on both ends
     i32 = 4k                  # 4096, support unit: k,m,g,t,p (case-insensitive)
     i32 = 032                 # octal, i32 = 26
     i32 = 0xff                # hexadecima, i32 = 255
     pi = 3.14159              # double type
```


## 11. Efficient streaming log library (log)

include: [co/log.h](https://github.com/idealvin/co/blob/master/include/co/log.h).

### 11.1 Basic introduction

The `log` library is a C++ streaming log library similar to [google glog](https://github.com/google/glog). Printing logs is more convenient and safer than the printf series of functions:

```cpp
LOG << "hello world" << 23;
```

The internal implementation of the log library uses an asynchronous method. Logs are first written to the cache. After a certain amount or more than a certain time, the background thread writes all logs to the file together. The performance is improved by 20 to 150 times compared to glog on different platforms.

The following table is the test result of continuously printing 1 million (about 50 bytes each) info level logs on different platforms:

| log vs glog | google glog | co/log |
| ------ | ------ | ------ |
| win2012 HDD | 1.6MB/s | 180MB/s |
| win10 SSD | 3.7MB/s | 560MB/s |
| mac SSD | 17MB/s | 450MB/s |
| linux SSD | 54MB/s | 1023MB/s |

### 11.2 Api Introduction

The log library provides only two API functions:
```cpp
void init();
void close();
```

`log:: init()` needs to be called once at the beginning of the `main` function. Since the log library depends on the flag library, the main function is generally written as follows:

```cpp
#include "co/flag.h"
#include "co/log.h"

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();
}
```

`log::close()` writes logs in the cache to a file, and exits the logging thread. The log library internally captures signals such as `SIGINT, SIGTERM, SIGQUIT`, and writes all cached logs to the log file before the program exits.

### 11.3 Print different levels of logs

Logs are divided into 5 levels: debug, info, warning, error and fatal. You can use the macros DLOG, LOG, WLOG, ELOG and FLOG to print 5 different levels of logs:

```cpp
DLOG << "this is DEBUG log " << 23;
LOG << "this is INFO log " << 23;
WLOG << "this is WARNING log " << 23;
ELOG << "this is ERROR log " << 23;
FLOG << "this is FATAL log " << 23;
```

Print the `fatal` log, which generally indicates that a fatal error occurred in the program. The log library will print the stacktrace information of the current thread and terminate the running of the program.

### 11.4 Conditional log (LOG_IF)

The log library also provides the `IF` version of the macro, which accepts a conditional parameter and prints the log when the specified condition is met.

- Code example

```cpp
DLOG_IF(cond) << "this is DEBUG log " << 23;
LOG_IF(cond) << "this is INFO log " << 23;
WLOG_IF(cond) << "this is WARNING log " << 23;
ELOG_IF(cond) << "this is ERROR log " << 23;
FLOG_IF(cond) << "this is FATAL log " << 23;
```

### 11.5 Print log every N times (LOG_EVERY_N)

The log library provides `LOG_EVERY_N` and other macros that support printing logs every N times. These macros use atomic operations internally to ensure thread safety.

- Code example

```cpp
// print log 1, 33, 65......
DLOG_EVERY_N(32) << "this is DEBUG log " << 23;
LOG_EVERY_N(32) << "this is INFO log " << 23;
WLOG_EVERY_N(32) << "this is WARNING log " << 23;
ELOG_EVERY_N(32) << "this is ERROR log " << 23;
```

FLOG does not have this function because the program is dead as soon as FLOG prints.

### 11.6 Print the first N logs (LOG_FIRST_N)

The log library provides `LOG_FIRST_N` and other macros to support printing the first N logs, these macros also use atomic operations internally to ensure thread safety.

- Code example

```cpp
// print the first 10 logs
DLOG_FIRST_N(10) << "this is DEBUG log " << 23;
LOG_FIRST_N(10) << "this is INFO log " << 23;
WLOG_FIRST_N(10) << "this is WARNING log " << 23;
ELOG_FIRST_N(10) << "this is ERROR log " << 23;
```

### 11.7 CHECK: Enhanced assert

The log library provides a series of CHECK macros, which can be regarded as enhanced asserts. These macros will not be cleared in DEBUG mode.

- Code example

```cpp
CHECK(1 + 1 == 2) << "say something here";
CHECK_EQ(1 + 1, 2);  // ==
CHECK_NE(1 + 1, 2);  // !=
CHECK_GE(1 + 1, 2);  // >=
CHECK_LE(1 + 1, 2);  // <=
CHECK_GT(1 + 1, 2);  // >  greater than
CHECK_LT(1 + 1, 2);  // <  less than
```

When CHECK failed, the LOG library will first call `log::close()` to write the logs, then print the stacktrace information of the current thread, and then exit the program.

### 11.8 Configuration items

- log_dir

  Specifies the log directory. The default is the `logs` directory under the current directory. It will be created automatically if it does not exist.

  ```cpp
  DEF_string(log_dir, "logs", "Log dir, will be created if not exists");
  ```

- log_file_name

  Specify the log file name (without path), which is empty by default, and use the program name as the log file name.

  ```cpp
  DEF_string(log_file_name, "", "name of log file, using exename if empty");
  ```

- min_log_level

  Specifies the minimum level of printing logs, which is used to disable low-level logs. The default is 0, and logs of all levels are printed.

  ```cpp
  DEF_int32(min_log_level, 0, "write logs at or above this level, 
                               0-4 (debug|info|warning|error|fatal)");
  ```

- max_log_file_size

  Specifies the maximum size of the log file. The default size is 256M. If the file size exceeds this size, a new log file is generated and the old log file is renamed.

  ```cpp
  DEF_int64(max_log_file_size, 256 << 20, "max size of log file, 
                                           default: 256MB");
  ```

- max_log_file_num

  Specifies the maximum number of log files. The default is 8. If this value is exceeded, old log files are deleted.

  ```cpp
  DEF_uint32(max_log_file_num, 8, "max number of log files");
  ```

- max_log_buffer_size

  Specifies the maximum size of the log cache. The default value is 32M. If this value is exceeded, half of the logs are discarded.

  ```cpp
  DEF_uint32(max_log_buffer_size, 32 << 20, "max size of log buffer, default: 32MB");
  ```

- cout

  Terminal log switch. The default is false. If true, also logging to the terminal.

  ```cpp
  DEF_bool (cout, false, "also logging to terminal");
  ```

### 11.9 Functional and performance test

The test code of the log library can be found in [test/log_test.cc](https://github.com/idealvin/co/blob/master/test/log_test.cc).

```sh
# run following commands in the root dir of co
xmake -b log    # build log

# print different types of logs
xmake r log

# also logging to terminal
xmake r log -cout

# min_log_level specify the minimum level of output logs
xmake r log -min_log_level=1   # 0-4: debug,info,warning,error,fatal 

# performance test, one thread continuously prints 1 million info-level logs
xmake r log -perf
```


## 12. Unit testing framework (unitest)

include: [co/unitest.h](https://github.com/idealvin/co/blob/master/include/co/unitest.h).

`unitest` is a unit testing framework similar to [google gtest](https://github.com/google/googletest), but easier to use.

### 12.1 Define test units and use cases

- Code example

```cpp
#include "co/unitest.h"
#include "co/os.h"

// Define a test unit named os with 3 different test cases.
// When running the unitest program, the parameter -os can be specified 
// to run only test cases in os.
DEF_test(os) {
    DEF_case(homedir) {
        EXPECT_NE(os::homedir(), std::string());
    }

    DEF_case(pid) {
        EXPECT_GE(os::pid(), 0);
    }

    DEF_case(cpunum) {
        EXPECT_GT(os::cpunum(), 0);
    }
}
```

### 12.2 Run test case

There are some unit test codes under [co/unitest](https://github.com/idealvin/co/tree/master/unitest), which can be compiled and executed as follows:

```sh
# run following commands in the root dir of co
xmake -b unitest    # build unitest or unitest.exe 

# run all test cases
xmake r unitest -a

# run only test cases in os unit
xmake r unitest -os
```


## 13. Efficient json library (json)

include: [co/json.h](https://github.com/idealvin/co/blob/master/include/co/json.h).

The `json` library is designed to be streamlined, efficient, and easy to use. It is comparable to [rapidjson](https://github.com/Tencent/rapidjson) in performance. If you use [jemalloc](https://github.com/jemalloc/jemalloc), the performance of `parse` and `stringify` will be further improved.

- Features of the json library
    - Supports 5 basic types: null, bool, int, double and string.
    - Supports two composite types, array and object.
    - All types are represented by a single Json class.
    - There is only one pointer data member in the Json class, `sizeof(Json) == sizeof(void*)`.
    - Json has a built-in reference count. The copy operation only increments the reference count (**atomic operation, thread-safe**), no memory copy is performed.
    - A built-in memory allocator (Jalloc) is used to optimize most memory allocation operations.

### 13.1 Basic types

- Code example

```cpp
Json x;                          // null
x.is_null();                     // determine if it is null

Json x = false;                  // bool type
x.is_bool();                     // determine whether it is bool type
bool b = x.get_bool();           // get value of bool type

Json x = 123;                    // int type
int i = x.get_int();             // get value of int type

Json x = (int64) 23;             // int type, 64-bit
int64 i = x.get_int64();         // returns a 64-bit integer

Json x = 3.14;                   // double type
double d = x.get_double();       // get value of double type

Json x = "hello world";          // string type
Json x(s, n);                    // string type (const char* s, size_t n)
x.is_string();                   // determine if it is a string type
x.size();                        // returns the length of the string
const char* s = x.get_string();  // returns pointer to a null-terminated string
```

### 13.2 Array type

`array` is an array type that can store any type of Json object.

```cpp
Json x = json::array();      // create an empty array, different from null
x.is_array();                // determine if it is an array
x.size();                    // returns the number of elements in the array
x.empty();                   // determine if the array is empty

Json x;                      // null, becomes array type after push_back is called
x.push_back(false);          // add a value of type bool
x.push_back(1);              // add a value of type int
x.push_back(3.14);           // add a value of type double
x.push_back("hello");        // add a value of type string
x.push_back(x);              // add a value of type array
x.push_back(obj);            // add a value of type object

// access members of array
x[0].get_bool();
x[1].get_int();

// traverse the array
for (uint32 i = 0; i < x.size(); ++i) {
    Json& v = x[i];
}
```

### 13.3 Object type

The `object` type is stored internally in the form of key-value. The value can be any type of Json object. The key has the following restrictions:

- key must be a C-style string ending in `'\0'`.
- The key cannot contain double quotes `"`.

```cpp
Json x = json::object();       // create an empty object, different from null
x.is_object();                 // determine if it is object type
x.size();                      // returns the number of elements in object
x.empty();                     // determine if object is empty

Json x;                        // null, becomes object after calling add_member()
x.add_member("name", "Bob");   // add string
x.add_member("age", 23);       // add int
x.add_member("height", 1.68);  // add double
x.add_member("array", array);  // add array
x.add_member("obj", obj);      // add object

x.has_member("name");          // determine if the member exists
x["name"].get_string();        // get value of the member

// return null if the key does not exist
Json v = x.find("age");
if (v.is_int()) v.get_int();

if (!(v = x.find("obj")).is_null()) {
    do_something();
}

// traverse the object
for (auto it = x.begin(); it != x.end(); ++it) {
    const char* key = it->key;  // key
    Json& v = it->value;        // value
}
```

### 13.4 Json to string

The Json class provides `str()` and `pretty()` methods to convert a Json object into a string:

```cpp
Json x;
fastring s = x.str();     // returns a string
fastring s = x.pretty();  // returns a pretty string

fastream fs;
fs << x;   // write json string to fastream
LOG << x;  // write json string to log file
```

In addition, the Json class also provides a `dbg()` method to convert Json into a debug string. The long strings inside Json object may be truncated:

```cpp
Json x;
fastring s = x.dbg();
LOG << x; // Actually equivalent to LOG << x.dbg();
```

### 13.5 String to json

`json::parse()` or the `parse_from()` method in the Json class can convert a string into a Json object:

```cpp
Json x;
fastring s = x.str();

// When parse fails, y is null
Json y = json::parse(s);
Json y = json::parse(s.data(), s.size());
y.parse_from(x.str());
```

### 13.6 Object type: adding and finding members efficiently

For the `object` type, an internal array is used to store the key-value pairs. This keeps the order in which members are added, but at the same time increases the cost of finding members. `operator[]` may be slow as it requires to find the member first.

- Use `add_member` instead of `operator[]` when adding members

  ```cpp
  // add members directly to the end without member searching.
  x.add_member("age", 23); // more efficient than x["age"] = 23
  ```

- Replace `operator[]` with `find` when finding members

  ```cpp
  // not so efficient member access with 3 search operations.
  if (x.has_member("age") && x["age"].is_int()) {
      int i = x["age"].get_int();
  }
  
  // using find(), need only one search operation.
  Json v = x.find("age");
  if (v.is_int()) {
      int i = v.get_int();
  }
  ```

### 13.7 Special characters in string types

The json string internally ends with '\0' and mustn't contain any binary character.

The json string supports escape characters such as `"` and `\`, as well as `\r, \n, \t`. However, containing these special characters will reduce the performance of `json::parse()`.

```cpp
Json x = "hello \r \n \t";  // ok, the string contains escape characters
Json x = "hello \" world";  // ok, the string contains "
Json x = "hello \\ world";  // ok, the string contains \ 
```


## 14. Time library (time)

include: [co/time.h](https://github.com/idealvin/co/blob/master/include/co/time.h).

### 14.1 monotonic time

`monotonic time` is implemented on most platforms as the time since system startup. It is generally used for timing, which is more stable than system time and is not affected by system time.

- Code example

```cpp
int64 us = now::us(); // Microsecond
int64 ms = now::ms(); // Millisecond
```

### 14.2 Time string (now::str())

`now::str()` is based on `strftime` and returns a string representation of the current system time in the specified format.

- Function prototype

```cpp
// fm: time output format
fastring str(const char* fm="%Y-%m-%d %H:%M:%S");
```

- Code example

```cpp
fastring s = now::str();     // "2018-08-08 08:08:08"
fastring s = now::str("%Y"); // "2028"
```

### 14.3 sleep

The Linux platform supports microsecond-level sleep, but it is difficult to implement on Windows. Therefore, only millisecond, second-level sleep is supported in this library.

- Code example

```cpp
sleep::ms(10); // sleep for 10 milliseconds
sleep::sec(1); // sleep for 1 second
```

### 14.4 Timer(Timer)

`Timer` is based on monotonic time. When a Timer object is created, it starts timing.

```cpp
Timer t;
sleep::ms(10);

int64 us = t.us(); // Microsecond
int64 ms = t.ms(); // Millisecond

t.restart();       // Restart timing
```


## 15. Thread library (thread)

include: [co/thread.h](https://github.com/idealvin/co/blob/master/include/co/thread.h).

### 15.1 Mutex

`Mutex` is a mutex commonly used in multi-threaded programming. At the same time, at most one thread owns the lock, and other threads must wait for the lock to be released.

There is also a kind of read-write lock, which allows multiple threads to read at the same time, but at most only one thread can write. In practical applications, its performance is poor, so this library has removed the read-write lock.

Corresponding to `Mutex`, there is a` MutexGuard` class for automatic acquisition and release of mutex locks.

- Code example

```cpp
Mutex m;
m.lock();         // Acquire the lock
m.unlock();       // Release the lock
m.try_lock();     // Acquire the lock

MutexGuard g(m);  // Call m.lock () in the constructor, 
                  // and call m.unlock () in the destructor.
```

### 15.2 SyncEvent

`SyncEvent` is a synchronization mechanism commonly used in multi-threaded programming and is suitable for the producer-consumer model.

- SyncEvent constructor description

```cpp
// manual_reset: whether to manually reset the synchronization status of events
// signaled:     whether the initial state of the event is signaled
SyncEvent(bool manual_reset=false, bool signaled=false);
```

- Code example

```cpp
SyncEvent ev;
ev.wait();                 // Thread A, waiting for event synchronization, 
                           // automatically reset the event to unsignaled.
ev.signal();               // Thread B, event synchronization notification

SyncEvent ev(true, false); // Enable manual_reset, waiting threads need 
                           // set synchronization status manually 
ev.wait(1000);             // Thread A, waiting 1000 milliseconds until 
                           //the event is synchronized or timed out
ev.reset();                // Thread A, manually set event status to unsignaled
ev.signal();               // Thread B, event synchronization notification
```

### 15.3 Thread

The `Thread` class is a wrapper around a thread. When a Thread object is created, the thread is started, and when the thread function ends, the thread automatically exits.

In addition to the constructor and destructor, the Thread class provides only two methods:

- `join()`, will block and wait for the thread function to finish executing, and then exit the thread.
- `detach()`, will not block, system resources are automatically released when the thread function ends.

- Code example

```cpp
// start thread
Thread x(f);                        // void f();
Thread x(f, p);                     // void f(void*);  void* p;
Thread x(&T::f, &t);                // void T::f();  T t;
Thread x(std::bind(f, 7));          // void f(int v);
Thread x(std::bind(&T::f, &t, 7));  // void T::f(int v);  T t;

// Block, wait for the thread function to finish executing
x.join();                           

// Starts the thread and destroys the Thread object.
// The thread runs independently of the Thread object.
Thread(f).detach();
```

### 15.4 Get the id of the current thread

`current_thread_id ()` is used to get the id of the current thread. The thread library uses [TLS] (https://wiki.osdev.org/Thread_Local_Storage) to save the thread id. Each thread only needs one system call.

- Notes

The Linux glibc has added the `gettid` system call since `2.30`. To avoid conflicts, the thread library has removed the earlier provided `gettid` interface and changed it to` current_thread_id`.

- Code example

```cpp
int id = current_thread_id();
```

### 15.5 thread_ptr based on TLS

`thread_ptr` is similar to`std::unique_ptr`, but uses the `TLS` mechanism internally. Each thread sets and has its own ptr.

- Code example

```cpp
struct T {
    void run() {
        cout << current_thread_id() << endl;
    }
};

thread_ptr<T> pt;

// Executed in the thread function of thread 1
if (pt == NULL) pt.reset(new T); 
pt->run();  // Print id of thread 1

// Executed in a thread function in thread 2
if (pt == NULL) pt.reset(new T);
pt->run();  // Print id of thread 2
```

### 15.6 Timed task scheduler (TaskSched)

The `TaskSched` class is used for scheduling scheduled tasks. All tasks are scheduled by a single thread internally, but tasks can be added from any thread.

- Methods provided by TaskSched
    - run_in
    - run_every
    - run_daily

```cpp
// @f: std::function<void()> type function object

// run f once after n seconds
void run_in(f, n);

// run f every n seconds
void run_every(f, n);

// run every day at hour:min:sec
// @hour: 0-23, default is 0
// @min:  0-59, default is 0
// @sec:  0-59, default is 0
void run_daily(f, hour=0, min=0, sec=0);
```

- Code example

```cpp
TaskSched s;                      // start task scheduling thread
s.run_in(f, 3);                   // run f once after 3 seconds    void f();
s.run_every(std::bind(f, 0), 3);  // run f every 3 seconds         void f(int);
s.run_daily(f);                   // run f every day at 00:00:00
s.run_daily(f, 23);               // run f every day at 23:00:00
s.run_daily(f, 23, 30);           // run f every day at 23:30:00
s.stop();                         // stop the task scheduling thread
```


## 16. Coroutine library (co)

include: [co/co.h](https://github.com/idealvin/co/blob/master/include/co/co.h).

### 16.1 Basic Concepts

- Coroutines are lightweight scheduling units that run in threads.
- Coroutines are to threads, similar to threads to processes.
- There can be multiple threads in a process and multiple coroutines in a thread.
- The thread where the coroutine runs in is generally called the scheduling thread.
- When a coroutine occurs such as io blocking or calling sleep, the scheduling thread will suspend the coroutine.
- When a coroutine is suspended, the scheduling thread will switch to other coroutines waiting to be executed.
- Switching of coroutines is performed in user mode, which is faster than switching between threads.

Coroutines are very suitable for writing network programs, and can achieve synchronous programming without asynchronous callbacks, which greatly reduces the programmer's ideological burden.

The `co` library implements a [golang](https://github.com/golang/go/) style coroutine with the following features:

- Built-in multiple scheduling threads, the default is number of the system CPU cores.
- The coroutines in the same scheduling thread share a stack. When a coroutine is suspended, the data on the stack will be copied out, and the data will be copied onto the stack when it is switched back. This method greatly reduces the memory footprint and millions of coroutines can be easily created on a single machine.
- There is a flat relationship between coroutines, and new coroutines can be created anywhere (including in coroutines).

The co coroutine library is based on [epoll](http://man7.org/linux/man-pages/man7/epoll.7.html), [kqueue](https://man.openbsd.org/kqueue.2), [iocp](https://docs.microsoft.com/en-us/windows/win32/fileio/io-completion-ports) implementation.

The relevant code for context switching in the co coroutine library is taken from [tbox](https://github.com/tboox/tbox/) by [ruki](https://github.com/waruqi), and tbox refers to the implementation of [boost](https://www.boost.org/doc/libs/1_70_0/libs/context/doc/html/index.html), thanks here!

### 16.2 Create coroutines (go)

`Golang` uses the keyword `go` to create coroutines. Similarly, the co library provides the `go()` method to create coroutines.

Creating a coroutine is similar to creating a thread. You need to specify a coroutine function. The first parameter of the `go()` method is the coroutine function:

```cpp
void go(void (*f)());
void go(void (*f)(void*), void* p);  // p specifies function parameters

template<typename T>
void go(void (T::*f)(), T* p);       // p binds a class T object

void go(const std::function<void()>& f);
```

Actual tests found that creating objects of type `std::function` is expensive, so go() is especially optimized for function types `void f()`,` void f(void *)`, `void T::f() `. In practical applications, these three types of functions should be used preferentially.

Strictly speaking, the go() method just allocates a `callback` to a scheduling thread. The actual creation of the coroutine is done by the scheduling thread. However, from the user's point of view, it can be logically considered that go() creates a coroutine and assigns it to a designated scheduling thread, waiting to be executed.

- Code example

```cpp
go(f);                       // void f();
go(f, p);                    // void f(void*);   void* p;
go(&T::f, p);                // void T::f();     T* p;
go(std::bind(f, 7));         // void f(int);
go(std::bind(&T::f, p, 7));  // void T::f(int);  T* p;
```

### 16.3 coroutine api

In addition to `go()`, the co-coroutine library also provides the following apis (located in namespace co):

```cpp
void sleep(unsigned int ms);
void stop();
int max_sched_num();
int sched_id();
int coroutine_id();
```

- When `sleep` is called in a coroutine, the scheduling thread will suspend the coroutine and switch to other coroutines waiting to be executed.
- `stop` will quit all scheduling threads, usually called before the process exits.
- `max_sched_num` returns the maximum number of supported scheduling threads. This value is currently the number of system CPU cores.
- `sched_id` returns the id of the currently scheduled thread, or -1 if the current thread is not a scheduling thread. The value range of id is 0 to `max_sched_num-1`.
- `coroutine_id` returns the id of the current coroutine, or -1 if the current thread is not a coroutine. Coroutines in different scheduling threads may have the same id.

- Code example

```cpp
// print the current sched_id and coroutine_id every 1 second
void f() {
    while (true) {
        co::sleep(1000);
        LOG << "sid: "<< co::sched_id() <<" cid: "<< co::coroutine_id();
    }
}

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    for (int i = 0; i <32; ++i) go(f);

    sleep::sec(8); // prevent the main thread from exiting immediately
    co::stop();    // stop all scheduling threads
    return 0;
}
```

### 16.4 Network Programming

co wraps commonly used socket APIs to support general network programming. All these APIs are in the `namespace co`, which must generally be called in a coroutine. Unlike the native APIs, when IO is blocked or sleep is called for these APIs, the scheduling thread will suspend the current coroutine and switch to other coroutines waiting to be executed.

#### 16.4.1 Commonly used socket APIs

Co provides some commonly used socket APIs:

```cpp
sock_t socket(int domain, int type, int proto);
sock_t tcp_socket(int af=AF_INET); // @af: address family, AF_INET, AF_INET6, etc
sock_t udp_socket(int af=AF_INET); // @af: address family, AF_INET, AF_INET6, etc

close shutdown bind listen accept getsockopt
recv recvfrom send sendto connect setsockopt

int recvn(sock_t fd, void* buf, int n, int ms=-1);
```

Most of the APIs provided by co are consistent with the native socket APIs, and their usage is almost the same, but there are some slight differences. The instructions are as follows:

- The `struct sockaddr*` in the native api parameters has been replaced with `void*`, avoiding the trouble of manual conversion.

- `socket`, `tcp_socket`, `udp_socket` are used to create sockets, the created sockets are non-blocking on linux/mac platforms, and [overlapped](https://support.microsoft.com/en- us/help/181611/socket-overlapped-io-versus-blocking-nonblocking-mode) on Windows.

- `close` can take an additional parameter `@ms` (default is 0), suspend the current coroutine for several milliseconds, and then close the socket.

- `shutdown` uses a single character `@c` to specify the direction, `'r'` turns off reading, `'w'` turns off writing, and both reading and writing are turned off by default.

```cpp
int shutdown(sock_t fd, char c='b');
```

- The socket returned by `accept` is non-blocking or overlapped, and does not need to be set by the user.

- `connect, recv, recvn, recvfrom, send, sendto` can take one more parameter specifying the timeout time `@ms` (default is -1). When a timeout occurs, these APIs return -1 and set errno to `ETIMEDOUT`.

- `recvn` receives `@n` bytes of tcp data, returns n after all received, 0 when disconnected, -1 for other errors.

- **Note:** accept, connect, recv, recvn, recvfrom, send and sendto must be called in coroutines.

- **Special attention:** `close` and `shutdown` will not block, but in order to complete the internal cleanup work normally, they must be called in the scheduling thread where the coroutine is located. Generally speaking, when performing recv and send operations in a coroutine, it is best to also call close and shutdown in this coroutine to close the socket.

The above api returns -1 when an error occurs, you can use `co::error()` to get the error code, and `co::strerror()` to see the error description.

#### 16.4.2 Common socket option settings

co provides the following APIs for setting commonly used socket options:

```cpp
void set_reuseaddr(sock_t fd);               // Set SO_REUSEADDR
void set_tcp_nodelay(sock_t fd);             // Set TCP_NODELAY
void set_tcp_keepalive(sock_t fd);           // Set SO_KEEPALIVE
void set_send_buffer_size(sock_t fd, int n); // Set the send buffer size
void set_recv_buffer_size(sock_t fd, int n); // Set the receive buffer size
```

#### 16.4.3 Other APIs

```cpp
// Fill in the ip address
bool init_ip_addr(struct sockaddr_in* addr, const char* ip, int port);
bool init_ip_addr(struct sockaddr_in6* addr, const char* ip, int port);

// Convert ip address to string
fastring ip_str(struct sockaddr_in* addr);
fastring ip_str(struct sockaddr_in6* addr);

// Send a RST, abnormally close the tcp connection, 
// avoid entering the timedwait state.
// @ms: The default is 0, suspending the coroutine 
// for a few milliseconds before send RST.
void reset_tcp_socket(sock_t fd, int ms=0);

int error();                   // return the current error code
const char* strerror();        // returns the current string error
const char* strerror(int err); // returns the string corresponding to @err
```

#### 16.4.4 hook system api

Calling the socket api of the co library in coroutines will not block, but the native socket API called in some third-party libraries may still block. In order to solve this problem, it's necessary to hook related system APIs.

The co library currently supports hooks on the linux/mac platform. The following is a list of hook functions:

```cpp
sleep usleep nanosleep

accept accept4 connect close shutdown
read readv recv recvfrom recvmsg
write writev send sendto sendmsg
select poll gethostbyaddr gethostbyname

gethostbyaddr_r gethostbyname2   // linux
gethostbyname_r gethostbyname2_r // linux

epoll_wait // linux
kevent     // mac
```

Users generally don't need to care about api hooks. If you are interested, you can read the source code implementation of [hook](https://github.com/idealvin/co/tree/master/src/co/impl).

#### 16.4.5 General network programming mode based on coroutines

Coroutines can achieve high-performance synchronous network programming. Taking the TCP program as an example, the server generally adopts a mode of one coroutine per connection, creating a new coroutine for each connection, and processing the data on the connection in the coroutine; the client does not need one connection per coroutine, connection pool is generally used instead, and multiple coroutines share the connections in the pool.

- The general mode of processing connection data on the server side:

```cpp
void server_fun() {
    while (true) {
        co::recv(...); // Receive client request data
        process(...);  // business processing
        co::send(...); // send the result to the client
    }
    co::close(...);    // close socket
}
```

- The general mode for the client to handle connection data:

```cpp
void client_fun() {
    co::send(...); // send request data to the server
    co::recv(...); // Receive server response data
    process(...);  // business processing
}
```

#### 16.4.6 Example of tcp server/client based on coroutine

- server code example

```cpp
struct Connection {
    sock_t fd;   // conn fd
    fastring ip; // peer ip
    int port;    // peer port
};

void on_new_connection(void* p) {
    std::unique_ptr<Connection> conn((Connection*)p);
    sock_t fd = conn->fd;
    co::set_tcp_keepalive(fd);
    co::set_tcp_nodelay(fd);
    
    char buf[8] = {0 };

    while (true) {
        int r = co::recv(fd, buf, 4);
        if (r == 0) {         // The client closed the connection
            co::close(fd);    // Call close to close the connection normally
            break;
        } else if (r == -1) { // Abnormal error, directly reset the connection
            co::reset_tcp_socket(fd, 1024);
            break;
        } else {
            LOG << "recv "<< buf;
            LOG << "send pong";
            co::send(fd, "pong", 4);
        }
    }
}

void server_fun() {
    sock_t fd = co::tcp_socket();
    co::set_reuseaddr(fd);

    sock_t connfd;
    int addrlen = sizeof(sockaddr_in);
    struct sockaddr_in addr;
    co::init_ip_addr(&addr, "127.0.0.1", 7788);

    co::bind(fd, &addr, sizeof(addr));
    co::listen(fd, 1024);

    while (true) {
        connfd = co::accept(fd, &addr, &addrlen);
        if (connfd == -1) continue;

        Connection* conn = new Connection;
        conn->fd = connfd;
        conn->ip = co::ip_str(&addr);
        conn->port = ntoh16(addr.sin_port);

        // Create a new coroutine for each connection
        co::go(on_new_connection, conn);
    }
}

go(server_fun); // Start server coroutine
```

- client code example

```cpp
void client_fun() {
    sock_t fd = co::tcp_socket();

    struct sockaddr_in addr;
    co::init_ip_addr(&addr, "127.0.0.1", 7788);

    co::connect(fd, &addr, sizeof(addr), 3000);
    co::set_tcp_nodelay(fd);

    char buf[8] = {0 };

    for (int i = 0; i <7; ++i) {
        co::sleep(1000);
        LOG << "send ping";
        co::send(fd, "ping", 4);
        co::recv(fd, buf, 4);
        LOG << "recv "<< buf;
    }

    co::close(fd);
}

go(client_fun); // Start client coroutine
```

### 16.5 Coroutine synchronization mechanism

The co library implements a synchronization mechanism similar to threads. Developers familiar with multithreaded programming can easily switch from threads to coroutine programming.

#### 16.5.1 Coroutine lock (co::Mutex)

`co::Mutex` is similar to `Mutex` in the thread library, except that it needs to be used in coroutine environment. When the acquisition of the lock fails, the scheduling thread will suspend the current coroutine, and the scheduling thread itself will not block.

In addition, co also provides a `co::MutexGuard` class, which is used in the same way as `MutexGuard` in the thread library.

- Code example

```cpp
co::Mutex mtx;
int v = 0;

void f1() {
    co::MutexGuard g(mtx);
    ++v;
}

void f2() {
    co::MutexGuard g(mtx);
    --v;
}

go(f1);
go(f2);
```

#### 16.5.2 Coroutine synchronization event (co::Event)

`co::Event` is similar to `SyncEvent` in the thread library, but it needs to be used in coroutine environment. When the `wait()` method is called, the scheduling thread will suspend the current coroutine, and the scheduling thread itself will not block.

- Code example

```cpp
co::Event ev;
int v = 0;

void f1() {
    // ev.wait(100); // wait for 100 ms
    ev.wait();       // wait forever
    if (v == 2) v = 1;
}

void f2() {
    v = 2;
    ev.signal();
}

go(f1);
go(f2);
```

### 16.6 Coroutine Pool

#### 16.6.1 co::Pool

Threads supports the `TLS` mechanism, and coroutines can also support a similar `CLS` mechanism, but considering that millions of coroutines may be created, CLS does not seem to be very efficient. The co library eventually gave up CLS and implemented `co::Pool` instead:

```cpp
class Pool {
  public:
    Pool();
    Pool(std::function<void*()>&& ccb, ​​std::function<void(void*)>&& dcb, 
         size_t cap=(size_t)-1);

    void* pop();
    void push(void* p);

  private:
    void* _p;
};
```

- Constructor

The parameters `ccb` and `dcb` in the second constructor can be used to create and destroy an element, and `cap` is used to specify the maximum capacity of the pool. The maximum capacity here is for a single thread. If cap is set to 1024 and there are 8 scheduling threads, the total maximum capacity is actually 8192. Also note that the maximum capacity is only valid when dcb is also specified.

- pop

This method pulls an element from the pool. When the pool is empty, if ccb is set, call ccb to create an element and return; if ccb is not set, return NULL.

- push

This method puts the element back into the pool. If the element is a NULL pointer, it is ignored. If the maximum capacity is exceeded and dcb is specified, call dcb directly to destroy the element without putting it into the pool.

The co::Pool class is coroutine safe. Calling the pop and push methods does not require locking, but it must be called in coroutines.

- Code example

```cpp
co::Pool p;

void f {
    Redis* rds = (Redis*) p.pop();    // pull a redis connection from the pool
    if (rds == NULL) rds = new Redis; // create a new redis connection

    rds->get("xx"); // call the get method of redis
    p.push(rds);    // put it back
}

go(f);
```

#### 16.6.2 co::PoolGuard

`co::PoolGuard` is a template class that pulls an element from co::Pool during construction and puts it back when destructuring. In addition, it also overloads `operator->`, so that we can use it like a smart pointer.

- Code example

```cpp
// Specify ccb, ​​dcb for automatic creation and destruction of Redis
co::Pool p(
    []() {return (void*) new Redis; }, // Specify ccb
    [](void* p) {delete (Redis*)p;}    // Specify dcb
);

void f() {
    co::PoolGuard<Redis> rds(p); // rds can be regarded as a Redis* pointer
    rds->get("xx");              // call the get method of redis
}

go(f);
```

With CLS mechanism, 100k coroutines needs to establish 100k connections. But using Pool, 100k coroutines may only need to share a small number of connections. Pool seems to be more efficient and reasonable than CLS, which is why this library does not support CLS.

### 16.7 Configuration items

The configuration items supported by the co library are as follows:

- co_sched_num

  Number of scheduling threads. The default is the number of system CPU cores. In the current implementation, this value must be <= the number of CPU cores.

- co_stack_size

  Coroutine stack size, default is 1MB. Each scheduling thread allocates a stack, and coroutines within the scheduling thread share this stack.

- co_max_recv_size

  The maximum data length that can be received at one time for `co::recv`. The default is 1M. If it exceeds this size, data will be received in multiple calls of `co::recv`.

- co_max_send_size

  The maximum data length that can be sent at one time for `co::send`. The default is 1M. If it exceeds this size, data will be sent in multiple calls of `co::send`.


## 17. Network library (so)

include: [co/so.h](https://github.com/idealvin/co/blob/master/include/co/so.h).

`so` is a network library based on coroutines, including three modules `tcp`, `http`, and `rpc`.

### 17.1 TCP programming

[so/tcp](https://github.com/idealvin/co/blob/master/include/co/so/tcp.h) module provides two classes `tcp::Server` and `tcp::Client`, which support both `ipv4` and `ipv6`, and can be used for general TCP programming.

#### 17.1.1 [tcp::Server](https://github.com/idealvin/co/blob/master/include/co/so/tcp.h)

```cpp
namespace tcp {
struct Connection {
    sock_t fd;   // conn fd
    fastring ip; // peer ip
    int port;    // peer port
    void* p;     // pointer to Server where this connection was accepted
};

class Server {
  public:
    Server(const char* ip, int port)
        : _ip((ip && *ip)? ip: "0.0.0.0"), _port(port) {
    }

    virtual ~Server() = default;

    virtual void start() {
        go(&Server::loop, this);
    }

    virtual void on_connection(Connection* conn) = 0;

  protected:
    fastring _ip;
    uint32 _port;

  private:
    void loop();
};
} // tcp
```

`tcp::Server` creates one coroutine for each connection. Calling the `start()` method enters the event loop. When a new connection is received, a coroutine is created, in which the `on_connection()` method is called to send or recv data on the connection.

This class can only be used as a base class. User must inherit this class and implement the `on_connection()` method. Note that the parameter `conn` is dynamically allocated, remember to delete it after using it.

[pingpong.cc](https://github.com/idealvin/co/blob/master/test/so/pingpong.cc) implements a simple pingpong server based on `tcp::Server`, readers can refer to its usage .

#### 17.1.2 [tcp::Client](https://github.com/idealvin/co/blob/master/include/co/so/tcp.h)

It is a tcp client class based on coroutines and needs to be used in coroutine environment. User needs to manually call the `connect()` method to establish a connection. It is recommended to determine whether the connection is established before calling `recv`, `send`. If not, call `connect()` to establish the connection. It is easy to achieve automatic reconnection in this way.

A `tcp::Client` corresponds to one connection, do not use the same tcp::Client object in multiple coroutines at the same time. The `co` coroutine library theoretically supports two coroutines using one connection at the same time, one recv, and one send, but it is not recommended to do so. The standard approach is that both recv and send are done in the same coroutine.

It does not need to create a connection for each coroutine. The recommended method is to put `tcp::Client` in `co::Pool`, and multiple coroutines share the connections in the pool. For each coroutine, a connection is taken from the pool when needed, and then put it back after use. This method can reduce the number of connections.

For specific usage of `tcp::Client`, readers can refer to `client_fun()` in [pingpong.cc](https://github.com/idealvin/co/blob/master/test/so/pingpong.cc). In addition, you can also refer to [http::Client](https://github.com/idealvin/co/blob/master/include/co/so/http.h) and [rpc::Client](https:// github.com/idealvin/co/blob/master/src/so/rpc.cc).

### 17.2 HTTP Programming

[so/http](https://github.com/idealvin/co/blob/master/include/co/so/http.h) module implements the `http::Server` and `http::Client` class based on `so/tcp` module, and it also provides a `so::easy()` method for quickly creating a static web server.

#### 17.2.1 Implement a simple http server

```cpp
http::Server serv("0.0.0.0", 80);

serv.on_req(
    [](const http::Req& req, http::Res& res) {
        if (req.is_method_get()) {
            if (req.url() == "/hello") {
                res.set_status(200);
                res.set_body("hello world");
            } else {
                res.set_status(404);
            }
        } else {
            res.set_status(501);
        }
    }
);

serv.start();
```

User only needs to specify the ip and port, call the `on_req()` method to register a callback for handling HTTP requests, and then the `start()` method can be called to start the server.

`co/test` provides a simple [demo](https://github.com/idealvin/co/blob/master/test/so/http_serv.cc), you can build and run it as follow:

```sh
xmake -b http_serv
xmake r http_serv
```

After starting `http_serv`, you can enter `127.0.0.1/hello` in the address bar of the browser to get the result.

#### 17.2.2 Implement a static web server

```cpp
#include "co/flag.h"
#include "co/log.h"
#include "co/so.h"

DEF_string(d, ".", "root dir"); // Specify the root directory of the web server

int main(int argc, char** argv) {
    flag::init(argc, argv);
    log::init();

    so::easy(FLG_d.c_str()); // mum never have to worry again

    return 0;
}
```

Readers can build [easy.cc](https://github.com/idealvin/co/blob/master/test/so/easy.cc) in `co/test` and run the web server as follow:

```sh
xmake -b easy
xmake r easy -d xxx # xxx as the root directory of the web server
```

#### 17.2.3 Usage of http client

```cpp
http::Client cli("www.xxx.com", 80);
http::Req req;
http::Res res;

req.set_method_get();
req.set_url("/");
cli.call(req, res); // Get the homepage of www.xxx.com

fastring s = res.body();
```

`http::Client` will automatically establish a connection in the `call()` method, user needn't call `connect()` manually. Note that `http::Client` must be used in coroutines.

`co/test` provides a simple [demo](https://github.com/idealvin/co/blob/master/test/so/http_cli.cc), build and run it as follow:

```sh
xmake -b http_cli
xmake r http_cli -ip=github.com -port=80
```

#### 17.2.4 Configuration items

- http_max_header_size

  Specify the maximum length of the http header part. The default is `4k`.

- http_max_body_size

  Specify the maximum length of the http body part. The default is `8M`.

- http_recv_timeout

  Specify the timeout time for http recv, in milliseconds. The default is `1024 ms`.

- http_send_timeout

  Specify the timeout time for http send, in milliseconds. The default is `1024 ms`.

- http_conn_timeout

  Specify the timeout time for http connect, in milliseconds. The default is `3000 ms`.

- http_conn_idle_sec

  Specify the timeout time for idle connections, in seconds. The default is `180` seconds.

- http_max_idle_conn

  Specify the maximum number of idle connections for http server. The default is `128`.

- http_log

  http log switch, the default is `true`. (Note that the log only prints the header of http)

### 17.3 rpc framework

[so/rpc](https://github.com/idealvin/co/blob/master/include/co/so/rpc.h) module implements an rpc framework based on `so/tcp`. It uses `tcp/json` as the transmission protocol internally. Simple tests show that single-threaded qps can reach `120k+`. Compared with struct-based binary protocols, json has at least the following advantages:

- It's easy to capture the package to see the transmitted json object, which is convenient for debugging.
- The rpc call directly transmits the json object without defining various structures, which greatly reduces the amount of codes.
- The rpc call parameters have the same form, fixed to `(const Json& req, Json& res)`, it is easy to generate code automatically.
- A general rpc client can be implemented, no need to generate different client codes for different rpc server.

#### 17.3.1 Introduction to rpc server interface

The interface of the rpc server is very simple:

```cpp
namespace rpc {
class Service {
  public:
    virtual ~Service() = default;
    virtual void process(const Json& req, Json& res) = 0; // business processing
};

class Server {
  public:
    virtual ~Server() = default;

    // start rpc server coroutine
    virtual void start() = 0;               

    // Service must be added before start()
    virtual void add_service(Service*) = 0; 
};

// Create an rpc server, if passwd is not empty, authentication is 
// required for every new client connection.
Server* new_server(const char* ip, int port, const char* passwd="");
} // rpc
```

`rpc::Server` receives client connections, creates a new coroutine for each connection, the new coroutine receives client requests, and then synchronously calls the `process()` method provided by `rpc::Service` to process the request, and finally send the results back to the client.

For specific business processing, you need to inherit `rpc::Service` and implement the `process()` method. In fact, the code of `process()` is automatically generated, and users only need to implement the specific rpc calling method.

#### 17.3.2 Introduction to rpc proto file

Here is a simple proto file `hello_world.proto`:

```cpp
// # or // for comments
package xx // namespace xx

service HelloWorld {
    hello,
    world,
}

hello.req {
    "method": "hello"
}

hello.res {
    "method": "hello",
    "err": 200,
    "errmsg": "200 ok"
}

world.req {
    "method": "world"
}

world.res {
    "method": "world",
    "err": 200,
    "errmsg": "200 ok"
}
```

`package xx` means to generate code into the namespace `xx`. You can also use `package xx.yy.zz` to generate nested namespaces.

`service HelloWorld` defines a service class that extends `rpc::Service`. `Hello, world` are two rpc methods it provides.

`hello.req, hello.res, world.req, world.res` are examples of request parameters and response results, which are not needed for generating code.

- Note that only one service can be defined in a proto file.

#### 17.3.3 rpc code generator

See [co/gen](https://github.com/idealvin/co/tree/master/gen) for souces of the code generator.

```sh
xmake -b gen           # build gen
gen hello_world.proto  # generator code of the rpc framework
```

Here is the generated C++ header `hello_world.h`:

```cpp
#pragma once

#include "co/so/rpc.h"
#include "co/hash.h"
#include <unordered_map>

namespace xx {

class HelloWorld: public rpc::Service {
  public:
    typedef void (HelloWorld::*Fun)(const Json&, Json&);

    HelloWorld() {
        _methods[hash64("ping")] = &HelloWorld::ping;
        _methods[hash64("hello")] = &HelloWorld::hello;
        _methods[hash64("world")] = &HelloWorld::world;
    }

    virtual ~HelloWorld() {}

    virtual void process(const Json& req, Json& res) {
        Json& method = req["method"];
        if (!method.is_string()) {
            res.add_member("err", 400);
            res.add_member("errmsg", "400 req has no method");
            return;
        }

        auto it = _methods.find(hash64(method.get_string(), method.size()));
        if (it == _methods.end()) {
            res.add_member("err", 404);
            res.add_member("errmsg", "404 method not found");
            return;
        }

        (this->*it->second)(req, res);
    }

    virtual void ping(const Json& req, Json& res) {
        res.add_member("method", "ping");
        res.add_member("err", 200);
        res.add_member("errmsg", "pong");
    }
    
    virtual void hello(const Json& req, Json& res) = 0;

    virtual void world(const Json& req, Json& res) = 0;

  private:
    std::unordered_map<uint64, Fun> _methods;
};

} // xx
```

You can see that `HelloWrold`'s constructor has registered the hello, world method to the internal map, and the `process()` method finds and calls the corresponding rpc method according to the `method` field in rpc request. Users only need to extend the `HelloWorld` class and implement the hello, world methods for specific business processing. Keep in mind that these methods may be called in different threads, and pay attention to thread safety in the implementations.

If you need to connect to other network services internally in the business process method, you may use coroutine-safe `co::Pool` to manage network connections.

The generated header file can be directly placed where the server code is located, as the client does not need it at all. The client only refers to the req/res definition in the proto file, which is enough for constructing req to make rpc calls.

#### 17.3.4 implement rpc server

The following example code `hello_world.cc` gives a simple implementation:

```cpp
#include "hello_world.h"

namespace xx {

class HelloWorldImpl: public HelloWorld {
  public:
    HelloWorldImpl() = default;
    virtual ~HelloWorldImpl() = default;

    virtual void hello(const Json& req, Json& res) {
        res.add_member("method", "hello");
        res.add_member("err", 200);
        res.add_member("errmsg", "200 ok");
    }

    virtual void world(const Json& req, Json& res) {
        res.add_member("method", "world");
        res.add_member("err", 200);
        res.add_member("errmsg", "200 ok");
    }
};

} // xx
```

Once implements the above business method, you can start the rpc server like this:

```cpp
rpc::Server* server = rpc::new_server("127.0.0.1", 7788, "passwd");
server->add_service(new xx::HelloWorldImpl);
server->start();
```

Note that calling the `start()` method will create a coroutine, and the server runs in the coroutine. Preventing the main thread from exiting is something the user needs to care about.

#### 17.3.5 rpc client

The interface of the rpc client is as follows:

```cpp
class Client {
  public:
    virtual ~Client() = default;
    virtual void ping() = 0; // send a heartbeat
    virtual void call(const Json& req, Json& res) = 0;
};

Client* new_client(const char* ip, int port, const char* passwd="");
} // rpc
```

`rpc::new_client()` creates an rpc client. If the server has set a password, the client needs to bring the password for authentication.

The `call()` method initiates an rpc call. Different rpc requests can be identified with the method field in req.

The `ping()` method is for sending heartbeats to the server.

- Notes
    - When `rpc::Client` was created, the connection was not established immediately. Instead, the connection was established when the first rpc call was made.
    - `delete rpc::Client` will close the connection. This operation must be performed within the coroutine.

Here is a simple rpc client example:

```cpp
void client_fun() {
    rpc::Client* c = rpc::new_client("127.0.0.1", 7788, "passwd");

    for (int i = 0; i < 10000; ++i) {
        Json req, res;
        req.add_member("method", "hello");
        c->call(req, res); // call the hello method
    }

    delete c; // delete in the coroutine to close the connection safely
}

int main (int argc, char** argv) {
    go(client_fun); // create coroutine
    while (1) sleep::sec(7);
    return 0;
}
```

Note that one `rpc::Client` corresponds to one connection. Do not use the same rpc::Client in multiple threads. In a multi-threaded environment, you can use `co::Pool` to manage client connections. Here is an example:

```cpp
co::Pool p(
    std::bind(&rpc::new_client, "127.0.0.1", 7788, "passwd"),
    [](void* p) { delete (rpc::Client*) p; }
);

void client_fun() {
    co::PoolGuard<rpc::Client> c(p);

    for (int i = 0; i < 10; ++i) {
        Json req, res;
        req.add_member("method", "hello");
        c->call(req, res); // call the hello method
    }
}

// create 8 coroutines
for (int i = 0; i < 8; ++i) {
    go(client_fun);
}
```

#### 17.3.6 Configuration Items

The rpc library supports the following configuration items:

- rpc_max_msg_size

  The maximum length of the rpc message. The default is `8M`.

- rpc_recv_timeout

  rpc timeout time for receiving data, unit is millisecond, default is `1024` milliseconds.

- rpc_send_timeout

  rpc timeout time for sending data, unit is millisecond, default is `1024` milliseconds.

- rpc_conn_timeout

  rpc connection timeout time in milliseconds. The default is `3000` milliseconds.

- rpc_conn_idle_sec

  The time during which rpc keeps idle connections in seconds. The default is `180` seconds. If a connection does not receive any data after this time, the server may close the connection.

- rpc_max_idle_conn

  The maximum number of idle connections. The default is `128`. If the number of connections exceeds this value, the server will close some idle connections (connections that have not received data within rpc_conn_idle_sec time).

- rpc_log

  Whether to enable rpc logs. The default is `true`.


## 18. Hash library

include: [co/hash.h](https://github.com/idealvin/co/blob/master/include/co/hash.h).

The `hash` library provides the following functions:

- hash64

  Calculates a 64-bit hash value. The murmur 2 hash algorithm is used internally.

- hash32

  Calculates a 32-bit hash value. The murmur 2 hash algorithm is used internally on 32 bit platform. For 64 bit platform, return the lower 32 bits of `hash64` directly.

- md5sum

  Calculates the md5 value of a string or a specified length of data and returns a 32-byte string.

- crc16

  Calculate the crc16 value of a string or a specified length of data, the implementation is taken from [redis](https://github.com/antirez/redis/).

- base64_encode

  Base64 encoding, `\r, \n` are not added, it is not necessary in practical.

- base64_decode

  Base64 decoding, throws `const char*` type exception when decoding fails.

- Code example

  ```cpp
  uint64 h = hash64(s);                // hash of string s
  uint64 h = hash64(s, n);             // hash of the specified length of data
  uint32 h = hash32(s);                // 32-bit hash of string s
  
  fastring s = md5sum("hello world");  // md5 of string, the result is 32 bytes
  uint16 x = crc16("hello world");     // crc16 of string
  
  fastring e = base64_encode(s);       // base64 encoding
  fastring d = base64_decode(e);       // base64 decoding
  ```


## 19. Path library

include: [co/path.h](https://github.com/idealvin/co/blob/master/include/co/path.h).

The `path` library is ported from [golang](https://github.com/golang/go/blob/master/src/path/path.go). The path separator is assumed to be '/'.

- `path::clean()`

  Returns the shortest equivalent form of a path. Continuous separators in the path are removed.

  ```cpp
  path::clean("./x//y/");    // return "x/y"
  path::clean("./x/..");     // return "."
  path::clean("./x/../..");  // return ".."
  ```

- `path::join()`

  Splices any number of strings into a complete path, the result was processed by path::clean().

  ```cpp
  path::join("x", "y", "z");  // return "x/y/z"
  path::join("/x/", "y");     // return "/x/y"
  ```

- `path::split()`

  Divide the path into two parts, dir and file. If the path does not contain a separator, the dir part is empty. The returned result satisfies the property `path = dir + file`.

  ```cpp
  path::split("/");     //-> {"/", ""}
  path::split("/a");    //-> {"/", "a"}
  path::split("/a/b");  //-> {"/a/", "b"}
  ```

- `path::dir()`

  Returns the directory portion of the path, the result was processed by path::clean().

  ```cpp
  path::dir("a");   // return "."
  path::dir("a/");  // return "a"
  path::dir("/");   // return "/"
  path::dir("/a");  // return "/";
  ```

- `path::base()`

  Returns the last element of the path.

  ```cpp
  path::base("");      // return "."
  path::base("/");     // return "/"
  path::base("/a/");   // return "a", ignores the trailing delimiter
  path::base("/a");    // return "a"
  path::base("/a/b");  // return "b"
  ```

- `path::ext()`

  This function returns the extension of the file name in the path.

  ```cpp
  path::ext("/a.cc");   // return ".cc"
  path::ext("/a.cc/");  // return ""
  ```


## 20. File System Operations (fs)

include: [co/fs.h](https://github.com/idealvin/co/blob/master/include/co/fs.h).

The `fs` library minimally implements common file system operations. The path separator for different platforms is recommended to use `'/'` uniformly.

### 20.1 Metadata Operations

- Code example

```cpp
bool x = fs::exists(path);  // determine if the file exists
bool x = fs::isdir(path);   // determine if the file is a directory
int64 x = fs::mtime(path);  // get the modification time of the file
int64 x = fs::fsize(path);  // get the size of the file

fs::mkdir("a/b");           // mkdir a/b
fs::mkdir("a/b", true);     // mkdir -p a/b

fs::remove("x/x.txt");      // rm x/x.txt
fs::remove("a/b");          // rmdir a/b, delete empty directory
fs::remove("a/b", true);    // rm -rf a/b

fs::rename("a/b", "a/c");   // rename
fs::symlink("/usr", "x");   // soft link: x -> /usr
```

### 20.2 Basic file read and write operations

The `fs` library implements the `fs::file` class, which supports basic read and write operations on files.

- Features of `fs::file`
    - Support `r, w, a, m` four read and write modes, the first three are consistent with fopen, `m` is similar to `w`, but the data of existing files is not cleared.
    - Does not support caching, read and write files directly.
    - Support `move` semantics, you can put the file object directly into the STL container.

- Code example

```cpp
fs::file f;               // f.open() can be called later to open the file
fs::file f("xx", 'r');    // Open the file in read mode

// automatically close the previously opened file
f.open("xx", 'a');        // append write, create if file does not exist
f.open("xx", 'w');        // normal write, create if file does not exist, 
                          // and clear the data if the file exists.
f.open("xx", 'm');        // modify write, create if file does not exist, 
                          // won't clear the data if the file exists.

if (f) f.read(buf, 512);  // read up to 512 bytes
f.write(buf, 32);         // write 32 bytes
f.write("hello");         // write string
f.write('c');             // write a single character
f.close();                // close the file, it will be called in destructor. 
```

### 20.3 File stream (fs::fstream)

`fs::file` does not support caching, and the performance of writing small files is poor. For this reason, the `fs` library also provides the `fs::fstream` class.

- Features of `fs::fstream`
    - For write only, and supports two modes: `w, a`.
    - You can customize the cache size. The default is `8k`.
    - Supports `move` semantics, allowing fstream objects to be placed in STL containers.

- Code example

```cpp
fs::fstream s;                     // default cache is 8k
fs::fstream s(4096);               // Specify cache as 4k
fs::fstream s("path", 'a');        // append mode, cache defaults to 8k
fs::fstream s("path", 'w', 4096);  // write mode, specify 4k cache

s.open("path", 'a');               // Open and close the previously opened file
if (s) s << "hello world" << 23;   // streaming
s.append(data, size);              // append data of specified length
s.flush();                         // write data from cache to file
s.close();                         // Close the file, it will be called in destructor
```


## 21. System operations (os)

include: [co/os.h](https://github.com/idealvin/co/blob/master/include/co/os.h).

```cpp
os::homedir();  // returns the home directory path
os::cwd();      // returns the current working directory path
os::exepath();  // returns the current process path
os::exename();  // returns the current process name
os::pid();      // returns the current process id
os::cpunum();   // returns the number of CPU cores
os::daemon();   // runs as a daemon, only supports Linux platforms
```



## Compiling

### xmake

[Xmake](https://github.com/xmake-io/xmake) is recommended for compiling the `CO` project.

- Compiler
    - Linux: [gcc 4.8+](https://gcc.gnu.org/projects/cxx-status.html#cxx11)
    - Mac: [clang 3.3+](https://clang.llvm.org/cxx_status.html)
    - Windows: [vs2015+](https://visualstudio.microsoft.com/)

- Install xmake

  For windows, mac and debian/ubuntu, you can directly go to the [xmake release page](https://github.com/xmake-io/xmake/releases) to download the installation package. For other systems, please refer to xmake's [installation instructions](https://xmake.io/#/guide/installation).

  Xmake disables compilation as root by default on linux. [ruki](https://github.com/waruqi) says it is not safe. You can add the following line to `~/.bashrc` to enable root compilation:

  ```sh
  export XMAKE_ROOT=y
  ```

- Quick start

  ```sh
  # All commands are executed in the root directory of co (the same below)
  xmake       # build libco and gen by default
  xmake -a    # build all projects (libco, gen, co/test, co/unitest)
  ```

- Build libco

  ```sh
  xmake build libco      # build libco only
  xmake -b libco         # the same as above
  ```

- Build and run unitest code

  [co/unitest](https://github.com/idealvin/co/tree/master/unitest) is unit test code that verifies the correctness of the functionality of the base library.

  ```sh
  xmake build unitest    # build can be abbreviated as -b
  xmake run unitest -a   # run all unit tests
  xmake r unitest -a     # the same as above
  xmake r unitest -os    # run unit test os
  xmake r unitest -json  # run unit test json
  ```

- Build and run test code

  [co/test](https://github.com/idealvin/co/tree/master/test) contains some test code. You can easily add a `xxx.cc` source file in the `co/test` directory, and then execute `xmake build xxx` to build it.

  ```sh
  xmake build flag             # flag.cc
  xmake build log              # log.cc
  xmake build json             # json.cc
  xmake build rapidjson        # rapidjson.cc
  xmake build rpc              # rpc.cc
  xmake build easy             # so/easy.cc
  xmake build pingpong         # so/pingpong.cc
  
  xmake r flag -xz             # test flag
  xmake r log                  # test log
  xmake r log -cout            # also log to terminal
  xmake r log -perf            # performance test
  xmake r json                 # test json
  xmake r rapidjson            # test rapidjson
  xmake r rpc                  # start rpc server
  xmake r rpc -c               # start rpc client
  xmake r easy -d xxx          # start web server
  xmake r pingpong             # pingpong server:   127.0.0.1:9988
  xmake r pingpong ip=::       # pingpong server:   :::9988  (ipv6)
  xmake r pingpong -c ip=::1   # pingpong client -> ::1:9988
  ```

- Build gen

  ```sh
  # It is recommended to put gen in the system directory (e.g. /usr/local/bin/).
  xmake build gen
  gen hello_world.proto
  ```

  Proto file format can refer to [hello_world.proto](https://github.com/idealvin/co/blob/master/test/__/rpc/hello_world.proto).

- Installation

  ```sh
  # Install header files, libco, gen by default.
  xmake install -o pkg         # package related files to the pkg directory
  xmake i -o pkg               # the same as above
  xmake install -o /usr/local  # install to the /usr/local directory
  ```

### cmake

[izhengfan](https://github.com/izhengfan) has helped to provide cmake support:  
- Build `libco` and `gen` by default.
- The library files are in the `build/lib` directory, and the executable files are in the `build/bin` directory.
- You can use `BUILD_ALL` to compile all projects.
- You can use `CMAKE_INSTALL_PREFIX` to specify the installation directory.

```sh
mkdir build && cd build
cmake ..
cmake .. -DBUILD_ALL=ON -DCMAKE_INSTALL_PREFIX=pkg
make -j8
make install
```


## Donate

<font face="Arial" size=3>
<img src="https://github.com/idealvin/docs/raw/master/img/wxzfb.png" alt="" align="center" width="668">
</font>