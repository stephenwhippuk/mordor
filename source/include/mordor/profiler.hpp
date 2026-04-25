#pragma once

// ---------------------------------------------------------------------------
// Mordor profiling hooks
//
// Lightweight scope-timer that records wall-time durations into a fixed-size
// ring buffer. Designed for frame-time tracking and coarse system budgets.
//
// All recording is ZERO-COST in release builds (MORDOR_PROFILING is defined
// only when NDEBUG is not set, or when explicitly opted-in at configure time).
//
// Usage:
//   // At the start of a system update:
//   MORDOR_PROFILE_SCOPE("simulation_tick");
//
//   // Query the last recorded duration for a named scope:
//   auto ns = mordor::profiler::last_ns("simulation_tick");
//
//   // Iterate all known scopes (for HUD display):
//   mordor::profiler::for_each([](std::string_view name, uint64_t ns){ ... });
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string_view>
#include <functional>

namespace mordor::profiler {

static constexpr std::size_t MAX_SCOPES = 64;

/// Returns the most recently recorded duration in nanoseconds for @p name.
/// Returns 0 if the scope has never been recorded.
uint64_t last_ns(std::string_view name);

/// Iterate all registered scope names and their last recorded duration.
void for_each(const std::function<void(std::string_view name, uint64_t ns)>& fn);

/// Reset all recorded durations (useful between test runs).
void reset();

namespace detail {

/// Internal: records @p elapsed_ns against @p name.
void record(std::string_view name, uint64_t elapsed_ns);

/// RAII scope timer. Prefer the MORDOR_PROFILE_SCOPE macro over direct use.
struct ScopeTimer
{
    explicit ScopeTimer(std::string_view name) noexcept;
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&)            = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&)                 = delete;
    ScopeTimer& operator=(ScopeTimer&&)      = delete;

private:
    std::string_view m_name;
    uint64_t         m_start_ns;
};

} // namespace detail
} // namespace mordor::profiler

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#if !defined(NDEBUG) || defined(MORDOR_PROFILING)
    // Unique variable name avoids shadowing in nested scopes.
    #define MORDOR_PROFILE_SCOPE(name) \
        ::mordor::profiler::detail::ScopeTimer _mordor_scope_##__LINE__{name}
#else
    #define MORDOR_PROFILE_SCOPE(name) ((void)0)
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
