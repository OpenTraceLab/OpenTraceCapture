#pragma once

#if defined(_MSC_VER)

/* usleep mapping:
 * - In C++: don't #define usleep(...) to avoid breaking 3rd-party headers.
 * - In C:   map POSIX name to our shim function implemented in otc_win_compat.h.
 */
  #ifdef __cplusplus
    /* Provide a helper name if you need it in your C++ code */
    extern void otc_usleep_win(unsigned long usec);
    static inline void otc_usleep(unsigned long usec) { otc_usleep_win(usec); }
  #else
    /* Implementation lives in otc_win_compat.h (forced via /FI) */
    #ifndef OTC_HAVE_USLEEP_WIN
    /* If the shim somehow wasn't included, declare it to satisfy the compiler. */
    extern void otc_usleep_win(unsigned long usec);
    #endif
    #ifndef usleep
    #define usleep(usec) otc_usleep_win((unsigned long)(usec))
    #endif
  #endif

/* Map other POSIX functions */
#include <windows.h>
#include <BaseTsd.h>   // SSIZE_T
#include <io.h>        // _read/_write/_close/_unlink
#include <direct.h>
#include <process.h>

// ssize_t on Windows
#ifndef ssize_t
typedef SSIZE_T ssize_t;
#endif

#define sleep(sec)   Sleep((DWORD)((sec) * 1000))
#define close  _close
#define read   _read
#define write  _write
#define unlink _unlink

// Case-insensitive compares
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

#endif /* _MSC_VER */
