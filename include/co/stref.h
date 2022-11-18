#pragma once

#include "god.h"
#include <string.h>
#include <ostream>

DEF_has_method(c_str);

namespace co {

class stref {
  public:
    constexpr stref() noexcept : _s(""), _n(0) {}
    constexpr stref(const char* s, size_t n) noexcept : _s(s), _n(n) {}

    template <typename S, god::enable_if_t<god::is_literal_string<god::remove_ref_t<S>>(), int> = 0>
    constexpr stref(S&& s) noexcept : _s(s), _n(sizeof(s) - 1) {}

    template <typename S, god::enable_if_t<god::is_c_str<god::remove_ref_t<S>>(), int> = 0>
    constexpr stref(S&& s) noexcept : _s(s), _n(strlen(s)) {}

    template <typename S, god::enable_if_t<god::has_method_c_str<S>(), int> = 0>
    constexpr stref(S && s) noexcept : _s(s.data()), _n(s.size()) {}

    constexpr const char* data() const noexcept { return _s; }
    constexpr size_t size() const noexcept { return _n; }

  private:
    const char* const _s;
    const size_t _n;
};

} // co

inline std::ostream& operator<<(std::ostream& os, const co::stref& s) {
    return os.write(s.data(), s.size());
}
