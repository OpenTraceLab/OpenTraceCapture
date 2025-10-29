#pragma once

#ifdef _WIN32

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

/* Include POSIX compatibility shim */
#include <opentracecapture/posix_compat.h>

#endif /* _WIN32 */
