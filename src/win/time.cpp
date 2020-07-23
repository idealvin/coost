#ifdef _WIN32

#include "co/time.h"
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace now {

struct _Mono {
    static inline int64 _QueryFrequency() {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return freq.QuadPart;
    }

    static inline int64 _QueryCounter() {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }

    static inline const int64& _Frequency() {
        static int64 freq = _QueryFrequency();
        return freq;
    }

    static inline int64 ms() {
        int64 count = _QueryCounter();
        const int64& freq = _Frequency();
        return (count / freq) * 1000 + (count % freq * 1000 / freq);
    }

    static inline int64 us() {
        int64 count = _QueryCounter();
        const int64& freq = _Frequency();
        return (count / freq) * 1000000 + (count % freq * 1000000 / freq);
    }
};

int64 ms() {
    return _Mono::ms();
}

int64 us() {
    return _Mono::us();
}

fastring str(const char* fm) {
    int64 x = time(0);
    struct tm t;
    _localtime64_s(&t, &x);

    char buf[32]; // 32 is big enough int most cases
    size_t r = strftime(buf, 32, fm, &t);
    return fastring(buf, r);
}

} // now

namespace ___ {
namespace sleep {

void ms(unsigned int n) {
    ::Sleep(n);
}

void sec(unsigned int n) {
    ::Sleep(n * 1000);
}

} // sleep
} // ___


struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    tmpres /= 10;  /*convert into microseconds*/
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

#endif
