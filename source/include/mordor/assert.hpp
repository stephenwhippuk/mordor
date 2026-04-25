#pragma once

// ---------------------------------------------------------------------------
// Mordor assertion facade
//
// MORDOR_ASSERT(condition)
//   In debug builds: logs a CRITICAL message then aborts.
//   In release builds: becomes [[assume(condition)]] — the optimizer can use
//   the hint but there is no runtime check or abort.
//
// MORDOR_ASSERT_MSG(condition, fmt, ...)
//   Same, but accepts a {fmt}-style message for additional context.
//
// These macros depend on mordor::log being initialised. If asserts fire
// before logging is up, the message falls back to stderr.
// ---------------------------------------------------------------------------

#include "mordor/log.hpp"

#include <cstdlib>
#include <cstdio>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#if defined(NDEBUG)
    // Release: give the optimiser the assume hint, no runtime cost.
    #if defined(__clang__) || defined(__GNUC__)
        #define MORDOR_ASSERT(cond)              __builtin_assume(!!(cond))
        #define MORDOR_ASSERT_MSG(cond, fmt, ...) __builtin_assume(!!(cond))
    #elif defined(_MSC_VER)
        #define MORDOR_ASSERT(cond)              __assume(!!(cond))
        #define MORDOR_ASSERT_MSG(cond, fmt, ...) __assume(!!(cond))
    #else
        #define MORDOR_ASSERT(cond)              ((void)(cond))
        #define MORDOR_ASSERT_MSG(cond, fmt, ...) ((void)(cond))
    #endif
#else
    #define MORDOR_ASSERT(cond)                                               \
        do {                                                                   \
            if (!!(cond)) { break; }                                           \
            MORDOR_LOG_CRITICAL("ASSERT FAILED: ({}) at {}:{}",               \
                #cond, __FILE__, __LINE__);                                    \
            mordor::log::flush();                                              \
            std::abort();                                                      \
        } while (false)

    #define MORDOR_ASSERT_MSG(cond, fmt, ...)                                  \
        do {                                                                   \
            if (!!(cond)) { break; }                                           \
            MORDOR_LOG_CRITICAL("ASSERT FAILED: ({}) at {}:{} — " fmt,        \
                #cond, __FILE__, __LINE__, ##__VA_ARGS__);                     \
            mordor::log::flush();                                              \
            std::abort();                                                      \
        } while (false)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
