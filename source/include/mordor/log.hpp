#pragma once

// ---------------------------------------------------------------------------
// Mordor logging facade
//
// This is the ONLY logging API engine code should use.
// Quill headers are an implementation detail; never include them elsewhere.
//
// If the backend needs to change (e.g. switch to spdlog):
//   1. Re-implement mordor::log::init / shutdown in log.cpp.
//   2. Re-map the MORDOR_LOG_* macros below to the new backend.
//   3. No other source file requires modification.
//
// Usage:
//   mordor::log::init();                           // once, at startup
//   MORDOR_LOG_INFO("Hello {}", 42);
//   mordor::log::shutdown();                       // once, at exit
// ---------------------------------------------------------------------------

#include <quill/Logger.h>
#include <quill/LogMacros.h>

namespace mordor::log {

/// Initialise the logging backend.
/// @param log_file  Optional path for a persistent log file.
///                  Pass nullptr to log to the console only.
void init(const char* log_file = nullptr);

/// Flush all pending log records synchronously.
void flush();

/// Flush and stop the logging backend. Call once before process exit.
void shutdown();

namespace detail {
    /// Returns the single engine-wide logger instance.
    /// Internal use only — call sites should use the MORDOR_LOG_* macros.
    quill::Logger* engine_logger();
} // namespace detail

} // namespace mordor::log

// ---------------------------------------------------------------------------
// Public logging macros
// These are thin wrappers around the quill LOG_* macros.
// All formatting is deferred to the background thread (zero hot-path cost).
//
// Spdlog migration guide (for future reference):
//   MORDOR_LOG_INFO(fmt, ...)  → SPDLOG_LOGGER_INFO(mordor::log::detail::engine_logger(), fmt, __VA_ARGS__)
//   etc. — only this block changes.
// ---------------------------------------------------------------------------

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define MORDOR_LOG_TRACE(fmt, ...)    LOG_TRACE_L1(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
#define MORDOR_LOG_DEBUG(fmt, ...)    LOG_DEBUG(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
#define MORDOR_LOG_INFO(fmt, ...)     LOG_INFO(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
#define MORDOR_LOG_WARN(fmt, ...)     LOG_WARNING(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
#define MORDOR_LOG_ERROR(fmt, ...)    LOG_ERROR(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
#define MORDOR_LOG_CRITICAL(fmt, ...) LOG_CRITICAL(mordor::log::detail::engine_logger(), fmt, ##__VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)
