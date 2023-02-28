#pragma once

#include "def.h"
#include "fastring.h"

namespace tscns {
class __coapi TSCNS {
public:
  static const int64 NsPerSec = 1000000000;

  void init(int64 init_calibrate_ns = 20000000,
            int64 calibrate_interval_ns = 3 * NsPerSec);

  void calibrate();

  static inline int64 rdtsc();

  inline int64 tsc2ns(int64 tsc) const;

  inline int64 rdns() const;

  static inline int64 rdsysns();

  double getTscGhz() const;

  // Linux kernel sync time by finding the first trial with tsc diff < 50000
  // We try several times and return the one with the mininum tsc diff.
  // Note that MSVC has a 100ns resolution clock, so we need to combine those ns
  // with the same value, and drop the first and the last value as they may not
  // scan a full 100ns range
  static void syncTime(int64 &tsc_out, int64 &ns_out);

  void saveParam(int64 base_tsc, int64 base_ns, int64 sys_ns,
                 double new_ns_per_tsc);

  alignas(64) std::atomic<uint32> param_seq_ {};
  double ns_per_tsc_;
  int64 base_tsc_;
  int64 base_ns_;
  int64 calibate_interval_ns_;
  int64 base_ns_err_;
  int64 next_calibrate_tsc_;
};

__coapi TSCNS &tscns();

// monotonic timestamp in seconds
__coapi uint64 sec();

// monotonic timestamp in milliseconds
__coapi uint64 ms();

// monotonic timestamp in microseconds
__coapi uint64 us();

// 2018-08-08 08:08:08.198855
__coapi fastring str();
} // namespace tscns

namespace now {

// monotonic timestamp in milliseconds
__coapi int64 ms();

// monotonic timestamp in microseconds
__coapi int64 us();

// "%Y-%m-%d %H:%M:%S" ==> 2018-08-08 08:08:08
__coapi fastring str(const char *fm = "%Y-%m-%d %H:%M:%S");

} // namespace now

namespace epoch {

// milliseconds since epoch
__coapi int64 ms();

// microseconds since epoch
__coapi int64 us();

} // namespace epoch

namespace ___ {
namespace sleep {

__coapi void ms(uint32 n);

__coapi void sec(uint32 n);

} // namespace sleep
} // namespace ___

using namespace ___;

class __coapi Timer {
public:
  Timer() { _start = now::us(); }

  void restart() { _start = now::us(); }

  int64 us() const { return now::us() - _start; }

  int64 ms() const { return this->us() / 1000; }

private:
  int64 _start;
};
