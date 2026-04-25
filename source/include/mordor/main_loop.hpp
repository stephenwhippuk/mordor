#pragma once

#include <functional>
#include <cstdint>

namespace mordor {

struct LoopConfig
{
    double fixed_tick_seconds{1.0 / 60.0};
    double max_frame_delta_seconds{0.25};
    double max_run_seconds{2.0};
};

struct LoopStats
{
    uint64_t simulation_steps{0};
    uint64_t render_frames{0};
};

struct LoopCallbacks
{
    std::function<void(double)> simulate;
    std::function<void(double)> render;
};

LoopStats run_main_loop(const LoopConfig& config, const LoopCallbacks& callbacks);

} // namespace mordor
