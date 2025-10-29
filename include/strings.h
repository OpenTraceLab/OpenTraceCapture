#pragma once

/* POSIX strings.h compatibility for Windows MSVC/clang-cl */
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__clang__))
  #include <string.h>

  #ifndef strcasecmp
    #define strcasecmp _stricmp
  #endif
  #ifndef strncasecmp
    #define strncasecmp _strnicmp
  #endif
#else
  /* Unix-likes have native strings.h */
  #include_next <strings.h>
#endif
