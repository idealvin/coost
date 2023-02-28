#ifdef _WIN32

#include "co/time.h"
#include <time.h>
#include <winsock2.h>  // for struct timeval
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace tscns {

void TSCNS::init(int64 init_calibrate_ns, int64 calibrate_interval_ns) {
  calibate_interval_ns_ = calibrate_interval_ns;
  int64 base_tsc, base_ns;
  syncTime(base_tsc, base_ns);
  int64 expire_ns = base_ns + init_calibrate_ns;
  while (rdsysns() < expire_ns)
    std::this_thread::yield();
  int64 delayed_tsc, delayed_ns;
  syncTime(delayed_tsc, delayed_ns);
  double init_ns_per_tsc =
      (double)(delayed_ns - base_ns) / (delayed_tsc - base_tsc);
  saveParam(base_tsc, base_ns, base_ns, init_ns_per_tsc);
}

void TSCNS::calibrate() {
  if (rdtsc() < next_calibrate_tsc_)
    return;
  int64 tsc, ns;
  syncTime(tsc, ns);
  int64 calulated_ns = tsc2ns(tsc);
  int64 ns_err = calulated_ns - ns;
  int64 expected_err_at_next_calibration =
      ns_err + (ns_err - base_ns_err_) * calibate_interval_ns_ /
                   (ns - base_ns_ + base_ns_err_);
  double new_ns_per_tsc =
      ns_per_tsc_ *
      (1.0 - (double)expected_err_at_next_calibration / calibate_interval_ns_);
  saveParam(tsc, calulated_ns, ns, new_ns_per_tsc);
}

inline int64 TSCNS::rdtsc() {
#ifdef _MSC_VER
  return __rdtsc();
#elif defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
  return __builtin_ia32_rdtsc();
#else
  return rdsysns();
#endif
}

inline int64 TSCNS::tsc2ns(int64 tsc) const {
  while (true) {
    uint32 before_seq = param_seq_.load(std::memory_order_acquire) & ~1;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    int64 ns = base_ns_ + (int64)((tsc - base_tsc_) * ns_per_tsc_);
    std::atomic_signal_fence(std::memory_order_acq_rel);
    uint32 after_seq = param_seq_.load(std::memory_order_acquire);
    if (before_seq == after_seq)
      return ns;
  }
}

inline int64 TSCNS::rdns() const { return tsc2ns(rdtsc()); }

inline int64 TSCNS::rdsysns() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(system_clock::now().time_since_epoch())
      .count();
}

double TSCNS::getTscGhz() const { return 1.0 / ns_per_tsc_; }

// Linux kernel sync time by finding the first trial with tsc diff < 50000
// We try several times and return the one with the mininum tsc diff.
// Note that MSVC has a 100ns resolution clock, so we need to combine those ns
// with the same value, and drop the first and the last value as they may not
// scan a full 100ns range
void TSCNS::syncTime(int64 &tsc_out, int64 &ns_out) {
#ifdef _MSC_VER
  const int N = 15;
#else
  const int N = 3;
#endif
  int64 tsc[N + 1];
  int64 ns[N + 1];

  tsc[0] = rdtsc();
  for (int i = 1; i <= N; i++) {
    ns[i] = rdsysns();
    tsc[i] = rdtsc();
  }

#ifdef _MSC_VER
  int j = 1;
  for (int i = 2; i <= N; i++) {
    if (ns[i] == ns[i - 1])
      continue;
    tsc[j - 1] = tsc[i - 1];
    ns[j++] = ns[i];
  }
  j--;
#else
  int j = N + 1;
#endif

  int best = 1;
  for (int i = 2; i < j; i++) {
    if (tsc[i] - tsc[i - 1] < tsc[best] - tsc[best - 1])
      best = i;
  }
  tsc_out = (tsc[best] + tsc[best - 1]) >> 1;
  ns_out = ns[best];
}

void TSCNS::saveParam(int64 base_tsc, int64 base_ns, int64 sys_ns,
                      double new_ns_per_tsc) {
  base_ns_err_ = base_ns - sys_ns;
  next_calibrate_tsc_ =
      base_tsc + (int64)((calibate_interval_ns_ - 1000) / new_ns_per_tsc);
  uint32 seq = param_seq_.load(std::memory_order_relaxed);
  param_seq_.store(++seq, std::memory_order_release);
  std::atomic_signal_fence(std::memory_order_acq_rel);
  base_tsc_ = base_tsc;
  base_ns_ = base_ns;
  ns_per_tsc_ = new_ns_per_tsc;
  std::atomic_signal_fence(std::memory_order_acq_rel);
  param_seq_.store(++seq, std::memory_order_release);
}

TSCNS &tscns() {
  static TSCNS tscns;
  return tscns;
}

// monotonic timestamp in seconds
uint64 sec() { return tscns().rdns() * 1e-9; }

// monotonic timestamp in milliseconds
uint64 ms() { return tscns().rdns() * 1e-6; }

// monotonic timestamp in microseconds
uint64 us() { return tscns().rdns() * 1e-3; }

// 2018-08-08 08:08:08.198855
fastring str() {
  const uint64 u = us();
  time_t x = u * 1e-06;
  struct tm t;
  localtime_r(&x, &t);
  char buf[256]{0};
  const size_t r = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
  snprintf(buf + r, sizeof(buf) - r, ".%06d", static_cast<int>(u % 1000000));
  return fastring(buf, r + 7);
}

} // namespace tscns


namespace now {
namespace _Mono {

inline int64 _QueryFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

inline int64 _QueryCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

inline const int64& _Frequency() {
    static int64 freq = _QueryFrequency();
    return freq;
}

inline int64 ms() {
    int64 count = _QueryCounter();
    const int64& freq = _Frequency();
    return (count / freq) * 1000 + (count % freq * 1000 / freq);
}

inline int64 us() {
    int64 count = _QueryCounter();
    const int64& freq = _Frequency();
    return (count / freq) * 1000000 + (count % freq * 1000000 / freq);
}

} // _Mono

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

    char buf[256];
    const size_t r = strftime(buf, sizeof(buf), fm, &t);
    return fastring(buf, r);
}

} // now

namespace epoch {

inline int64 filetime() {
    FILETIME ft;
    LARGE_INTEGER x;
    GetSystemTimeAsFileTime(&ft);
    x.LowPart = ft.dwLowDateTime;
    x.HighPart = ft.dwHighDateTime;
    return x.QuadPart - 116444736000000000ULL;
}

int64 ms() {
    return filetime() / 10000;
}

int64 us() {
    return filetime() / 10;
}

} // epoch

namespace ___ {
namespace sleep {

void ms(uint32 n) {
    ::Sleep(n);
}

void sec(uint32 n) {
    ::Sleep(n * 1000);
}

} // sleep
} // ___

#endif
