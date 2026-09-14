#pragma once

namespace co {

void _assert_failed(const char* c, const char* file, int line, const char* e);

} // co

#define runtime_assert(c, ...) \
do { \
    if (c) break; \
    co::_assert_failed(#c, __FILE__, __LINE__, ""#__VA_ARGS__); \
} while (0);
