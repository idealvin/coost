#pragma once

#include <atomic>

namespace co {
//     |
//     v   ^
//---------|-----  release barrier
//
//---------|-----  acquire barrier
//     ^   v
//     |
using memorder_t = std::memory_order;
constexpr memorder_t mo_relaxed = std::memory_order_relaxed;
constexpr memorder_t mo_consume = std::memory_order_consume;
constexpr memorder_t mo_acquire = std::memory_order_acquire;
constexpr memorder_t mo_release = std::memory_order_release;
constexpr memorder_t mo_acq_rel = std::memory_order_acq_rel;
constexpr memorder_t mo_seq_cst = std::memory_order_seq_cst;

using std::atomic_thread_fence;

// mo: mo_relaxed, mo_consume, mo_acquire, mo_seq_cst
template<typename T>
inline T atomic_load(const T* p, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->load(mo);
}

// mo: mo_relaxed, mo_release, mo_seq_cst
template<typename T, typename V>
inline void atomic_store(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    ((std::atomic<T>*)p)->store((T)v, mo);
}

// mo: all
template<typename T, typename V>
inline T atomic_swap(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->exchange((T)v, mo);
}

// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V>
inline T atomic_compare_swap(T* p, O o, V v, memorder_t smo=mo_seq_cst, memorder_t fmo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    T x = (T)o;
    ((std::atomic<T>*)p)->compare_exchange_strong(x, (T)v, smo, fmo);
    return x;
}

// the same as atomic_compare_swap
// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V>
inline T atomic_cas(T* p, O o, V v, memorder_t smo=mo_seq_cst, memorder_t fmo=mo_seq_cst) {
    return atomic_compare_swap(p, o, v, smo, fmo);
}

// like the atomic_cas, but return true if the swap operation is successful
// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V>
inline bool atomic_bool_cas(T* p, O o, V v, memorder_t smo=mo_seq_cst, memorder_t fmo=mo_seq_cst) {
    return atomic_cas(p, o, v, smo, fmo) == (T)o;
}

template<typename T, typename V>
inline T atomic_add(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_add(v, mo) + v;
}

template<typename T, typename V>
inline T atomic_sub(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_sub(v, mo) - v;
}

template<typename T>
inline T atomic_inc(T* p, memorder_t mo=mo_seq_cst) {
    return atomic_add(p, 1, mo);
}

template<typename T>
inline T atomic_dec(T* p, memorder_t mo=mo_seq_cst) {
    return atomic_sub(p, 1, mo);
}

template<typename T, typename V>
inline T atomic_fetch_add(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_add(v, mo);
}

template<typename T, typename V>
inline T atomic_fetch_sub(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_sub(v, mo);
}

template<typename T>
inline T atomic_fetch_inc(T* p, memorder_t mo=mo_seq_cst) {
    return atomic_fetch_add(p, 1, mo);
}

template<typename T>
inline T atomic_fetch_dec(T* p, memorder_t mo=mo_seq_cst) {
    return atomic_fetch_sub(p, 1, mo);
}

template<typename T, typename V>
inline T atomic_or(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_or((T)v, mo) | (T)v;
}

template<typename T, typename V>
inline T atomic_and(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_and((T)v, mo) & (T)v;
}

template<typename T, typename V>
inline T atomic_xor(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_xor((T)v, mo) ^ (T)v;
}

template<typename T, typename V>
inline T atomic_fetch_or(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_or((T)v, mo);
}

template<typename T, typename V>
inline T atomic_fetch_and(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_and((T)v, mo);
}

template<typename T, typename V>
inline T atomic_fetch_xor(T* p, V v, memorder_t mo=mo_seq_cst) {
    static_assert(sizeof(T) == sizeof(std::atomic<T>), "");
    static_assert(alignof(T) == alignof(std::atomic<T>), "");
    return ((std::atomic<T>*)p)->fetch_xor((T)v, mo);
}

} // co
