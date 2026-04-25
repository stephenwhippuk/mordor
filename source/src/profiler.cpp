#include "mordor/profiler.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string_view>

namespace {

struct ScopeEntry
{
    char     name[64]{};
    uint64_t last_ns{0};
};

// Fixed-size flat array: no heap, no locks for reads.
// Entries are added during startup/warmup; the steady-state path only writes
// last_ns which is a single 64-bit store.
struct ScopeTable
{
    std::array<ScopeEntry, mordor::profiler::MAX_SCOPES> entries{};
    std::atomic<std::size_t> count{0};
};

ScopeTable g_table;

ScopeEntry* find_or_insert(std::string_view name)
{
    const std::size_t current = g_table.count.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < current; ++i)
    {
        if (std::string_view{g_table.entries[i].name} == name)
        {
            return &g_table.entries[i];
        }
    }
    // Insert — safe because scope names are registered at startup.
    const std::size_t idx = g_table.count.fetch_add(1, std::memory_order_acq_rel);
    if (idx >= mordor::profiler::MAX_SCOPES)
    {
        return nullptr; // table full — silently drop
    }
    auto& entry = g_table.entries[idx];
    std::size_t copy_len = name.size() < sizeof(entry.name) - 1
        ? name.size()
        : sizeof(entry.name) - 1;
    std::memcpy(entry.name, name.data(), copy_len);
    entry.name[copy_len] = '\0';
    return &entry;
}

uint64_t now_ns()
{
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock::now().time_since_epoch()).count());
}

} // namespace

namespace mordor::profiler {

uint64_t last_ns(std::string_view name)
{
    const std::size_t count = g_table.count.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (std::string_view{g_table.entries[i].name} == name)
        {
            return g_table.entries[i].last_ns;
        }
    }
    return 0;
}

void for_each(const std::function<void(std::string_view, uint64_t)>& fn)
{
    const std::size_t count = g_table.count.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < count; ++i)
    {
        fn(g_table.entries[i].name, g_table.entries[i].last_ns);
    }
}

void reset()
{
    const std::size_t count = g_table.count.load(std::memory_order_acquire);
    for (std::size_t i = 0; i < count; ++i)
    {
        g_table.entries[i].last_ns = 0;
    }
}

namespace detail {

void record(std::string_view name, uint64_t elapsed_ns)
{
    ScopeEntry* e = find_or_insert(name);
    if (e != nullptr)
    {
        e->last_ns = elapsed_ns;
    }
}

ScopeTimer::ScopeTimer(std::string_view name) noexcept
    : m_name{name}
    , m_start_ns{now_ns()}
{}

ScopeTimer::~ScopeTimer()
{
    record(m_name, now_ns() - m_start_ns);
}

} // namespace detail
} // namespace mordor::profiler
