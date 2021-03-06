/*
 * nvbio
 * Copyright (C) 2012-2014, NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <nvbio/basic/types.h>

#ifndef WIN32
#ifdef __INTEL_COMPILER
#include <ia32intrin.h> // ia32intrin.h
#else
//#warning atomics.h BROKEN on GCC!
// Intrinsics docs at http://gcc.gnu.org/onlinedocs/gcc-4.3.2/gcc/Atomic-Builtins.html
#endif
#endif

namespace nvbio {

/// \page atomics_page Atomics
///
/// This module implements basic host atomic counters:
///
/// - AtomicInt32
/// - AtomicInt64
///

///@addtogroup Basic
///@{

///@defgroup Atomics
/// This module implements basic host/device atomic counters.
///@{

int32  host_atomic_add( int32* value, const  int32 op);
uint32 host_atomic_add(uint32* value, const uint32 op);
int64  host_atomic_add( int64* value, const  int64 op);
uint64 host_atomic_add(uint64* value, const uint64 op);

int32  host_atomic_sub( int32* value, const  int32 op);
uint32 host_atomic_sub(uint32* value, const uint32 op);
int64  host_atomic_sub( int64* value, const  int64 op);
uint64 host_atomic_sub(uint64* value, const uint64 op);

NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
int32 atomic_add(int32* value, const int32 op)
{
  #if defined(NVBIO_DEVICE_COMPILATION)
    return atomicAdd( value, op );
  #else
    return host_atomic_add( value, op );
  #endif
}

NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
uint32 atomic_add(uint32* value, const uint32 op)
{
  #if defined(NVBIO_DEVICE_COMPILATION)
    return atomicAdd( value, op );
  #else
    return host_atomic_add( value, op );
  #endif
}

NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
uint64 atomic_add(uint64* value, const uint64 op)
{
  #if defined(NVBIO_DEVICE_COMPILATION)
    return atomicAdd( value, op );
  #else
    return host_atomic_add( value, op );
  #endif
}

NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
int32 atomic_sub(int32* value, const int32 op)
{
  #if defined(NVBIO_DEVICE_COMPILATION)
    return atomicSub( value, op );
  #else
    return host_atomic_sub( value, op );
  #endif
}

NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
uint32 atomic_sub(uint32* value, const uint32 op)
{
  #if defined(NVBIO_DEVICE_COMPILATION)
    return atomicSub( value, op );
  #else
    return host_atomic_sub( value, op );
  #endif
}

#if defined(WIN32)

int32 atomic_increment(int32 volatile *value);
int64 atomic_increment(int64 volatile *value);

int32 atomic_decrement(int32 volatile *value);
int64 atomic_decrement(int64 volatile *value);

/// an atomic integer class
/// TODO: redo in int64
template<typename intT>
struct AtomicInt
{
    /// constructor
    AtomicInt() : m_value(0) {}

    /// destructor
    AtomicInt(const intT value) : m_value(value) {}

    /// increment by one
    intT increment()
    {
        return atomic_increment( &m_value );
    }

    /// decrement by one
    intT decrement()
    {
        return atomic_decrement( &m_value );
    }

    /// increment by one
    intT operator++(int) { return increment()-1u; }

    /// decrement by one
    intT operator--(int) { return decrement()+1u; }

    /// increment by one
    intT operator++() { return increment(); }

    /// decrement by one
    intT operator--() { return decrement(); }

    /// increment by one
    intT operator+=(const intT v) { return m_value += v; }
    /// increment by one
    intT operator-=(const intT v) { return m_value -= v; }

    /// compare
    bool operator==(const intT value) { return m_value == value; }
    bool operator!=(const intT value) { return m_value != value; }
    bool operator>=(const intT value) { return m_value >= value; }
    bool operator<=(const intT value) { return m_value <= value; }
    bool operator>(const intT value)  { return m_value >  value; }
    bool operator<(const intT value)  { return m_value <  value; }

    volatile intT m_value;
};

typedef AtomicInt<int>     AtomicInt32;
typedef AtomicInt<int64>   AtomicInt64;

#else

/// an atomic integer class
struct AtomicInt32
{
    /// constructor
    AtomicInt32() : m_value(0) {}

    /// destructor
    AtomicInt32(const int value) : m_value(value) {}

#define USE_ASM 1
#if USE_ASM
    // look here
    // http://www.memoryhole.net/kyle/2007/05/atomic_incrementing.html
    // and also figure if this could use xaddq for the 64bit version
    /// postincrement by one
    int operator++(int) {
        int tmp;

    	__asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (tmp), "=m" (m_value)
			      : "0" (1), "m" (m_value));

    	return tmp;
    }

    /// postdecrement by one
    int operator--(int) {
        int tmp;

        __asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (tmp), "=m" (m_value)
			      : "0" (-1), "m" (m_value));

    	return tmp;
    }

    /// preincrement by one
    int operator++() {
        int tmp;

    	__asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (tmp), "=m" (m_value)
			      : "0" (1), "m" (m_value));

    	return tmp + 1;
    }

    /// predecrement by one
    int operator--() {
        int tmp;

        __asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (tmp), "=m" (m_value)
			      : "0" (-1), "m" (m_value));

        return tmp - 1;
    }

    /// preincrement
    int operator+=(int value) {
        register int v asm ("eax") = value;

        __asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "r" (v), "m" (m_value));

        return v + value;
    }

    /// predecrement
    int operator-=(int value ) {
        register int v asm ("eax") = -value;

        __asm__ __volatile__ ("lock; xaddl %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "r" (v), "m" (m_value));

        return v - value;
    }
#elif defined(__INTEL_COMPILER)
    /// postincrement by one
    int operator++(int) { return _InterlockedIncrement( (void*)&m_value )-1u; }

    /// postdecrement by one
    int operator--(int) { return _InterlockedDecrement( (void*)&m_value )+1u; }

    /// preincrement by one
    int operator++() { return _InterlockedIncrement( (void*)&m_value ); }

    /// predecrement by one
    int operator--() { return _InterlockedDecrement( (void*)&m_value ); }

    /// preincrement
    int operator+=(int value) { return __sync_add_and_fetch(&m_value, value); }

    /// predecrement
    int operator-=(int value) { return __sync_sub_and_fetch(&m_value, value); }
#else
    /// postincrement by one
    int operator++(int) { return __sync_fetch_and_add(&m_value, 1); }

    /// postdecrement by one
    int operator--(int) { return __sync_fetch_and_add(&m_value, -1); }

    /// preincrement by one
    int operator++() { return __sync_add_and_fetch(&m_value, 1); }

    /// predecrement by one
    int operator--() { return __sync_add_and_fetch(&m_value, -1); }

    /// preincrement
    int operator+=(int value) { return __sync_add_and_fetch(&m_value, value); }

    /// predecrement
    int operator-=(int value) { return __sync_sub_and_fetch(&m_value, value); }
#endif

    /// comparisons
    bool operator==(const int value) { return m_value == value; }
    bool operator!=(const int value) { return m_value != value; }
    bool operator>=(const int value) { return m_value >= value; }
    bool operator<=(const int value) { return m_value <= value; }
    bool operator>(const int value)  { return m_value >  value; }
    bool operator<(const int value)  { return m_value <  value; }

    volatile int m_value;
};

struct AtomicInt64
{
    /// constructor
    AtomicInt64() : m_value(0) {}

    /// destructor
    AtomicInt64(const long value) : m_value(value) {}

#define USE_ASM 1
#if USE_ASM
    // look here
    // http://www.memoryhole.net/kyle/2007/05/atomic_incrementing.html
    // and also figure if this could use xaddq for the 64bit version
    /// postincrement by one
    long operator++(int) {
        register long v asm ("eax");

    	__asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "0" (1), "m" (m_value));

    	return v;
    }

    /// postdecrement by one
    long operator--(int) {
        register long v asm ("eax");

        __asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "0" (-1), "m" (m_value));

    	return v;
    }

    /// preincrement by one
    long operator++() {
        register long v asm ("eax");

    	__asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "0" (1), "m" (m_value));

    	return v + 1;
    }

    /// predecrement by one
    long operator--() {
        register long v asm ("eax");

        __asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "0" (-1), "m" (m_value));

        return v - 1;
    }

    /// preincrement
    long operator+=(long value) {
        register long v asm ("eax") = value;

        __asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "r" (v), "m" (m_value));

        return v + value;
    }

    /// predecrement
    long operator-=(long value ) {
        register long v asm ("eax") = -value;

        __asm__ __volatile__ ("lock; xaddq %0, %1"
			      : "=r" (v), "=m" (m_value)
			      : "r" (v), "m" (m_value));

        return v - value;
    }
#elif defined(__INTEL_COMPILER)
#error TODO!
#else
#error TODO!
#endif

    /// comparisons
    bool operator==(const long value) { return m_value == value; }
    bool operator!=(const long value) { return m_value != value; }
    bool operator>=(const long value) { return m_value >= value; }
    bool operator<=(const long value) { return m_value <= value; }
    bool operator>(const long value)  { return m_value >  value; }
    bool operator<(const long value)  { return m_value <  value; }

    volatile long m_value;
};

#undef USE_ASM
#endif

/// comparisons
inline bool operator==(const long a, const AtomicInt64& b) { return a == b.m_value; }
inline bool operator!=(const long a, const AtomicInt64& b) { return a != b.m_value; }
inline bool operator>=(const long a, const AtomicInt64& b) { return a >= b.m_value; }
inline bool operator<=(const long a, const AtomicInt64& b) { return a <= b.m_value; }
inline bool operator> (const long a, const AtomicInt64& b) { return a > b.m_value; }
inline bool operator< (const long a, const AtomicInt64& b) { return a < b.m_value; }

inline bool operator==(const int a, const AtomicInt32& b) { return a == b.m_value; }
inline bool operator!=(const int a, const AtomicInt32& b) { return a != b.m_value; }
inline bool operator>=(const int a, const AtomicInt32& b) { return a >= b.m_value; }
inline bool operator<=(const int a, const AtomicInt32& b) { return a <= b.m_value; }
inline bool operator> (const int a, const AtomicInt32& b) { return a > b.m_value; }
inline bool operator< (const int a, const AtomicInt32& b) { return a < b.m_value; }

///@} Atomics
///@} Basic

} // namespace nvbio
