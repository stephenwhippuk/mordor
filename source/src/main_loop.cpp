#include "mordor/main_loop.hpp"

#include "mordor/assert.hpp"

#include <chrono>
#include <thread>

namespace mordor {

LoopStats run_main_loop(const LoopConfig& config, const LoopCallbacks& callbacks)
{
    MORDOR_ASSERT_MSG(config.fixed_tick_seconds > 0.0, "fixed_tick_seconds must be > 0");
    MORDOR_ASSERT_MSG(config.max_frame_delta_seconds > 0.0, "max_frame_delta_seconds must be > 0");
    MORDOR_ASSERT_MSG(config.max_run_seconds > 0.0, "max_run_seconds must be > 0");
    MORDOR_ASSERT_MSG(
        config.max_frame_delta_seconds >= config.fixed_tick_seconds,
        "max_frame_delta_seconds must be >= fixed_tick_seconds");

    using clock = std::chrono::steady_clock;

    auto last = clock::now();
    const auto start = last;

    double accumulator = 0.0;
    LoopStats stats{};

    while (std::chrono::duration<double>(clock::now() - start).count() < config.max_run_seconds)
    {
        const auto now = clock::now();
        double frame_delta = std::chrono::duration<double>(now - last).count();
        last = now;

        if (frame_delta > config.max_frame_delta_seconds)
        {
            frame_delta = config.max_frame_delta_seconds;
        }

        accumulator += frame_delta;

        while (accumulator >= config.fixed_tick_seconds)
        {
            callbacks.simulate(config.fixed_tick_seconds);
            accumulator -= config.fixed_tick_seconds;
            ++stats.simulation_steps;
        }

        const double alpha = accumulator / config.fixed_tick_seconds;
        callbacks.render(alpha);
        ++stats.render_frames;

        // Small sleep prevents busy-spin in this bootstrap loop.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return stats;
}

} // namespace mordor
