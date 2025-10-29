/*
 * This file is part of the libopentracecapture project.
 *
 * Windows compatibility header - ensures proper header ordering
 */

#ifndef OPENTRACECAPTURE_WIN_COMPAT_H
#define OPENTRACECAPTURE_WIN_COMPAT_H

#if defined(_WIN32)
  /* Target Windows 7+ for IPv6 support */
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0601
  #endif
  #ifndef WINVER
  #define WINVER 0x0601
  #endif

  /* Keep Windows headers lean and avoid min/max macro collisions */
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif

  /* Include winsock2 before windows.h - this MUST come first */
  #include <winsock2.h>
  #include <ws2tcpip.h>
  
  /* Now safe to include windows.h - it will skip winsock.h due to header guards */
  #include <windows.h>
#endif

#endif
