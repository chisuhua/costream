#pragma once

#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include <assert.h>
#include <iostream>
#include <string>
#include <algorithm>

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#define PASTE2(x, y) x##y
#define PASTE(x, y) PASTE2(x, y)

#ifdef NDEBUG
#define debug_warning(exp)                                                                         \
  do {                                                                                             \
  } while (false)
#else
#define debug_warning(exp)                                                                         \
  do {                                                                                             \
    if (!(exp))                                                                                    \
      fprintf(stderr, "Warning: " STRING(exp) " in %s, " __FILE__ ":" STRING(__LINE__) "\n",       \
              __PRETTY_FUNCTION__);                                                                \
  } while (false)
#endif

#ifdef NDEBUG
#define debug_print(fmt, ...)                                                                      \
  do {                                                                                             \
  } while (false)
#else
#define debug_print(fmt, ...)                                                                      \
  do {                                                                                             \
    fprintf(stderr, fmt, ##__VA_ARGS__);                                                           \
  } while (false)
#endif
#ifdef NDEBUG
#define ifdebug if (false)
#else
#define ifdebug if (true)
#endif

extern int debug_level;
#define debug_level_print(level, fmt, ...) \
	do { if (level <= debug_level) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#define DEBUG_LEVEL_DEFAULT	-1
#define DEBUG_LEVEL_ERR		3
#define DEBUG_LEVEL_WARNING	4
#define DEBUG_LEVEL_INFO	6
#define DEBUG_LEVEL_DEBUG	7
#define ERROR(fmt, ...) \
	debug_level_print(DEBUG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) \
	debug_level_print(DEBUG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) \
	debug_level_print(DEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define DEBUG(fmt, ...) \
	debug_level_print(DEBUG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)


// #include "atomic_helpers.h"
