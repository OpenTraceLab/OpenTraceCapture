/*
 * This file is part of the libopentracecapture project.
 *
 * Windows compatibility header - ensures proper header ordering
 */

#ifndef OPENTRACECAPTURE_WIN_COMPAT_H
#define OPENTRACECAPTURE_WIN_COMPAT_H

#if defined(_WIN32)
  /* Prevent windows.h from including winsock.h */
  #ifndef _WINSOCKAPI_
  #define _WINSOCKAPI_
  #endif

  /* Keep Windows headers lean and avoid min/max macro collisions */
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif

  /* Ensure winsock2 takes precedence over legacy winsock */
  #include <winsock2.h>
  #include <ws2tcpip.h>

  /* Now safe to include windows.h */
  #include <windows.h>
#endif

#endif
