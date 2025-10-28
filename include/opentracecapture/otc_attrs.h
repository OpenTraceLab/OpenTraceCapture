/*
 * This file is part of the libopentracecapture project.
 *
 * Compiler attribute portability macros
 */

#ifndef OPENTRACECAPTURE_ATTRS_H
#define OPENTRACECAPTURE_ATTRS_H

#if defined(_MSC_VER)
  /* MSVC doesn't understand GCC __attribute__ syntax */
  #ifndef __GNUC__
  #define __attribute__(x)
  #endif

  #define OTC_ALWAYS_INLINE __forceinline
  #define OTC_NOINLINE      __declspec(noinline)
  #define OTC_DEPRECATED    __declspec(deprecated)
  #define OTC_ALIGN(x)      __declspec(align(x))
  #define OTC_PACKED

#else /* GCC/Clang */
  #define OTC_ALWAYS_INLINE __attribute__((always_inline)) inline
  #define OTC_NOINLINE      __attribute__((noinline))
  #define OTC_DEPRECATED    __attribute__((deprecated))
  #define OTC_ALIGN(x)      __attribute__((aligned(x)))
  #define OTC_PACKED        __attribute__((packed))
#endif

#endif
