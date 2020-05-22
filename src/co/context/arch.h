/*!The Treasure Box Library
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * Copyright (C) 2009 - 2018, TBOOX Open Source Group.
 *
 * @author      ruki
 * @file        arch.h
 *
 * modified by Alvin at 2018/12/21
 */
#pragma once

#if defined(__i386) || \
    defined(__i686) || \
    defined(__i386__) || \
    defined(__i686__) || \
    defined(_M_IX86)
  #define ARCH_X86

#elif defined(__x86_64) || \
      defined(__amd64__) || \
      defined(__amd64) || \
      defined(_M_IA64) || \
      defined(_M_X64)
  #define ARCH_X64

#elif defined(__arm64) || \
      defined(__arm64__) || \
      (defined(__aarch64__) && __aarch64__)
  #define ARCH_ARM
  #define ARCH_ARM64

#elif defined(__arm__) 
  #define ARCH_ARM

#elif defined(mips) || \
      defined(_mips) || \
      defined(__mips__)
  #define ARCH_MIPS

#else
  #error unknown arch
#endif

// ARM version
#ifdef ARCH_ARM
  #if defined(__ARM64_ARCH_8__)
    #define ARCH_ARM_v8
  #elif defined(__ARM_ARCH_7A__)
    #define ARCH_ARM_v7A
  #elif defined(__ARM_ARCH_7__)
    #define ARCH_ARM_v7
  #elif defined(__ARM_ARCH_6__)
    #define ARCH_ARM_v6
  #elif defined(__ARM_ARCH_5TE__)
    #define ARCH_ARM_v5te
  #elif defined(__ARM_ARCH_5__)
    #define ARCH_ARM_v5
  #elif defined(__ARM_ARCH_4T__)
    #define ARCH_ARM_v4t
  #elif defined(__ARM_ARCH_3__)
    #define ARCH_ARM_v3
  #elif defined(__ARM_ARCH)
    #if __ARM_ARCH >= 8
      #define ARCH_ARM_v8
    #elif __ARM_ARCH >= 7
      #define ARCH_ARM_v7
    #elif __ARM_ARCH >= 6
      #define ARCH_ARM_v6
    #else
      #define ARCH_ARM_v5
    #endif
  #elif defined(__aarch64__) && __aarch64__
    #define ARCH_ARM_v8
  #else 
    #error unknown arm arch version
  #endif

  #ifdef __ARM_NEON__
    #define ARCH_ARM_NEON
  #endif 
#endif

// VFP
#ifdef __VFP_FP__
  #define ARCH_VFP
#endif

// cpu byte size
#if defined(__LP64__) || \
    defined(__64BIT__) || \
    defined(_LP64) || \
    defined(__x86_64) || \
    defined(__x86_64__) || \
    defined(__amd64) || \
    defined(__amd64__) || \
    defined(__arm64) || \
    defined(__arm64__) || \
    defined(__sparc64__) || \
    defined(__PPC64__) || \
    defined(__ppc64__) || \
    defined(__powerpc64__) || \
    defined(_M_X64) || \
    defined(_M_AMD64) || \
    defined(_M_IA64) || \
    (defined(__WORDSIZE) && (__WORDSIZE == 64)) 
  #define CPU_BIT_SIZE       (64)
  #define CPU_BYTE_SIZE      (8)
#else
  #define CPU_BIT_SIZE       (32)
  #define CPU_BYTE_SIZE      (4)
#endif
