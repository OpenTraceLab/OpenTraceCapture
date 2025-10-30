#pragma once

#ifdef _WIN32

#ifndef OTC_WIN_SHIM_ONCE
#define OTC_WIN_SHIM_ONCE 1

/* Keep Windows headers lean and avoid std::min/max hijack */
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#  define NOMINMAX 1
#endif

/* Target: Windows 7+ (same you used before) */
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#  define WINVER 0x0601
#endif

/* IMPORTANT: winsock2.h must come BEFORE windows.h */
#include <winsock2.h>
#include <ws2tcpip.h>

/* Block winsock.h from being included later via windows.h or others */
#ifndef _WINSOCKAPI_
#  define _WINSOCKAPI_
#endif

#include <windows.h>

/* Nuke noisy Win32 macros that collide with C++ libs */
#ifdef ERROR
#  undef ERROR
#endif
#ifdef interface
#  undef interface
#endif

#if defined(_MSC_VER)
/* MSVC: no ftello/fseeko; use 64-bit variants. */
#  include <stdio.h>
#  include <stdint.h>

#  ifndef fseeko
#    define fseeko _fseeki64
#  endif
#  ifndef ftello
#    define ftello _ftelli64
#  endif

/* Optional: provide a consistent 64-bit file offset type for your code. */
typedef int64_t otc_off_t;

/* ---- usleep shim (implementation lives here) ---- */
#ifndef OTC_HAVE_USLEEP_WIN
#define OTC_HAVE_USLEEP_WIN 1
static inline void otc_usleep_win(unsigned long usec) {
    /* round up to next millisecond */
    Sleep((DWORD)((usec + 999UL) / 1000UL));
}
#endif

/* ---- gettimeofday shim ---- */
#if !defined(OTC_HAVE_GETTIMEOFDAY)
#define OTC_HAVE_GETTIMEOFDAY 1

static inline int gettimeofday(struct timeval *tv, void *tz_unused) {
    (void)tz_unused;
    FILETIME ft;
    ULARGE_INTEGER uli;

    /* 100-ns intervals since Jan 1, 1601 (UTC) */
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    /* Convert to microseconds since Unix epoch (Jan 1, 1970) */
    const unsigned long long EPOCH_DIFF_US = 11644473600000000ULL;
    unsigned long long usec = (uli.QuadPart / 10ULL) - EPOCH_DIFF_US;

    tv->tv_sec  = (long)(usec / 1000000ULL);
    tv->tv_usec = (long)(usec % 1000000ULL);
    return 0;
}
#endif

#else
/* POSIX/MinGW path: keep normal ftello/fseeko; ensure 64-bit off_t. */
#  include <sys/types.h>
typedef off_t otc_off_t;
#endif

#endif /* OTC_WIN_SHIM_ONCE */

/* Include POSIX compatibility shim */
#include <opentracecapture/posix_compat.h>

#endif /* _WIN32 */
