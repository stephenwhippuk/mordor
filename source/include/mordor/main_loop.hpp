#pragma once

#include <cstdint>
#include <functional>

namespace mordor {

struct LoopConfig
{
    double m_fixed_tick_seconds{1.0 / 60.0};
    double m_max_frame_delta_seconds{0.25};
    double m_max_run_seconds{2.0};
};

struct LoopStats
{
    uint64_t m_simulation_steps{0};
    uint64_t m_render_frames{0};
};

struct LoopCallbacks
{
    std::function<void(double)> m_simulate;
    std::function<void(double)> m_render;
    std::function<bool()>       m_should_continue;
};

LoopStats run_main_loop(const LoopConfig& config, const LoopCallbacks& callbacks);

} // namespace mordor
