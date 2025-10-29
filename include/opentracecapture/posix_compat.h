#pragma once
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__clang__))

// Standard Windows + CRT bits
#include <windows.h>
#include <BaseTsd.h>   // SSIZE_T
#include <io.h>        // _read/_write/_close/_unlink
#include <direct.h>
#include <process.h>

// ssize_t on Windows
#ifndef ssize_t
typedef SSIZE_T ssize_t;
#endif

// Sleep helpers (usleep() expects microseconds)
static inline void otc_usleep_win(unsigned long usec) {
    // Sleep() granularity is ms; round up
    DWORD ms = (DWORD)((usec + 999) / 1000);
    Sleep(ms);
}

// Map common POSIX functions/macros
#define usleep(usec) otc_usleep_win((unsigned long)(usec))
#define sleep(sec)   Sleep((DWORD)((sec) * 1000))

#define close  _close
#define read   _read
#define write  _write
#define unlink _unlink

// Case-insensitive compares
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

#endif /* _WIN32 && (_MSC_VER || __clang__) */
