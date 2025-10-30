#pragma once

#if defined(_MSC_VER)
#  ifdef OPENTRACECAPTURECXX_BUILD
#    define OTCCXX_API __declspec(dllexport)
#  else
#    define OTCCXX_API __declspec(dllimport)
#  endif
#else
#  define OTCCXX_API
#endif
