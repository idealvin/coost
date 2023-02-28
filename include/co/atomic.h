#pragma once

#ifdef _MSC_VER
#include <atomic>
#include <type_traits>

using memory_order_t = std::memory_order;
#define mo_relaxed std::memory_order_relaxed
#define mo_consume std::memory_order_consume
#define mo_acquire std::memory_order_acquire
#define mo_release std::memory_order_release
#define mo_acq_rel std::memory_order_acq_rel
#define mo_seq_cst std::memory_order_seq_cst

template <typename T>
using _if_match_atomic_t = typename std::enable_if<sizeof(T) == sizeof(std::atomic<T>), int>::type;

// mo: mo_relaxed, mo_consume, mo_acquire, mo_seq_cst
template<typename T, _if_match_atomic_t<T> = 0>
inline T atomic_load(const T* p, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->load(mo);
}

// mo: mo_relaxed, mo_release, mo_seq_cst
template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline void atomic_store(T* p, V v, memory_order_t mo = mo_seq_cst) {
    ((std::atomic<T>*)p)->store((T)v, mo);
}

// mo: all
template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_swap(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->exchange((T)v, mo);
}

// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_compare_swap(T* p, O o, V v, memory_order_t smo = mo_seq_cst, memory_order_t fmo = mo_seq_cst) {
    T x = (T)o;
    ((std::atomic<T>*)p)->compare_exchange_strong(x, (T)v, smo, fmo);
    return x;
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_add(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_add(v, mo) + v;
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_sub(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_sub(v, mo) - v;
}

template<typename T, _if_match_atomic_t<T> = 0>
inline T atomic_inc(T* p, memory_order_t mo = mo_seq_cst) {
    return atomic_add(p, 1, mo);
}

template<typename T, _if_match_atomic_t<T> = 0>
inline T atomic_dec(T* p, memory_order_t mo = mo_seq_cst) {
    return atomic_sub(p, 1, mo);
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_add(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_add(v, mo);
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_sub(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_sub(v, mo);
}

template<typename T, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_inc(T* p, memory_order_t mo = mo_seq_cst) {
    return atomic_fetch_add(p, 1, mo);
}

template<typename T, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_dec(T* p, memory_order_t mo = mo_seq_cst) {
    return atomic_fetch_sub(p, 1, mo);
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_or(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_or((T)v, mo) | (T)v;
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_and(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_and((T)v, mo) & (T)v;
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_xor(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_xor((T)v, mo) ^ (T)v;
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_or(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_or((T)v, mo);
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_and(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_and((T)v, mo);
}

template<typename T, typename V, _if_match_atomic_t<T> = 0>
inline T atomic_fetch_xor(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return ((std::atomic<T>*)p)->fetch_xor((T)v, mo);
}

#else /* gcc/clang */

//     |
//     v   ^
//---------|-----  release barrier
//
//---------|-----  acquire barrier
//     ^   v
//     |
enum memory_order_t {
    mo_relaxed = __ATOMIC_RELAXED,
    mo_consume = __ATOMIC_CONSUME,
    mo_acquire = __ATOMIC_ACQUIRE,
    mo_release = __ATOMIC_RELEASE,
    mo_acq_rel = __ATOMIC_ACQ_REL,
    mo_seq_cst = __ATOMIC_SEQ_CST,
};

// mo: mo_relaxed, mo_consume, mo_acquire, mo_seq_cst
template <typename T>
inline T atomic_load(T* p, memory_order_t mo = mo_seq_cst) {
    return __atomic_load_n(p, mo);
}

// mo: mo_relaxed, mo_release, mo_seq_cst
template <typename T, typename V>
inline void atomic_store(T* p, V v, memory_order_t mo = mo_seq_cst) {
    __atomic_store_n(p, (T)v, mo);
}

// mo: all
template <typename T, typename V>
inline T atomic_swap(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_exchange_n(p, (T)v, mo);
}

// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template <typename T, typename O, typename V>
inline T atomic_compare_swap(T* p, O o, V v, memory_order_t smo = mo_seq_cst, memory_order_t fmo = mo_seq_cst) {
    T x = (T)o;
    __atomic_compare_exchange_n(p, &x, (T)v, false, smo, fmo);
    return x;
}

template <typename T>
inline T atomic_inc(T* p, memory_order_t mo = mo_seq_cst) {
    return __atomic_add_fetch(p, 1, mo);
}

template <typename T>
inline T atomic_dec(T* p, memory_order_t mo = mo_seq_cst) {
    return __atomic_sub_fetch(p, 1, mo);
}

template <typename T, typename V>
inline T atomic_add(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_add_fetch(p, v, mo);
}

template <typename T, typename V>
inline T atomic_sub(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_sub_fetch(p, v, mo);
}

template <typename T>
inline T atomic_fetch_inc(T* p, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_add(p, 1, mo);
}

template <typename T>
inline T atomic_fetch_dec(T* p, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_sub(p, 1, mo);
}

template <typename T, typename V>
inline T atomic_fetch_add(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_add(p, v, mo);
}

template <typename T, typename V>
inline T atomic_fetch_sub(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_sub(p, v, mo);
}

template <typename T, typename V>
inline T atomic_or(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_or_fetch(p, v, mo);
}

template <typename T, typename V>
inline T atomic_and(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_and_fetch(p, v, mo);
}

template <typename T, typename V>
inline T atomic_xor(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_xor_fetch(p, v, mo);
}

template <typename T, typename V>
inline T atomic_fetch_or(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_or(p, v, mo);
}

template <typename T, typename V>
inline T atomic_fetch_and(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_and(p, v, mo);
}

template <typename T, typename V>
inline T atomic_fetch_xor(T* p, V v, memory_order_t mo = mo_seq_cst) {
    return __atomic_fetch_xor(p, v, mo);
}

#endif

// the same as atomic_compare_swap
// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V>
inline T atomic_cas(T* p, O o, V v, memory_order_t smo = mo_seq_cst, memory_order_t fmo = mo_seq_cst) {
    return atomic_compare_swap(p, o, v, smo, fmo);
}

// like the atomic_cas, but return true if the swap operation is successful
// smo: success memory order, all
// fmo: failure memory order, cannot be mo_release, mo_acq_rel,
//      and cannot be stronger than smo
template<typename T, typename O, typename V>
inline bool atomic_bool_cas(T* p, O o, V v, memory_order_t smo = mo_seq_cst, memory_order_t fmo = mo_seq_cst) {
    return atomic_cas(p, o, v, smo, fmo) == (T)o;
}
