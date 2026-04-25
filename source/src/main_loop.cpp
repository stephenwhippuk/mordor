#include "mordor/main_loop.hpp"

#include "mordor/assert.hpp"

#include <chrono>
#include <thread>

namespace mordor {

LoopStats run_main_loop(const LoopConfig& config, const LoopCallbacks& callbacks)
{
    MORDOR_ASSERT_MSG(config.m_fixed_tick_seconds > 0.0, "fixed_tick_seconds must be > 0");
    MORDOR_ASSERT_MSG(config.m_max_frame_delta_seconds > 0.0, "max_frame_delta_seconds must be > 0");
    MORDOR_ASSERT_MSG(config.m_max_run_seconds > 0.0, "max_run_seconds must be > 0");
    MORDOR_ASSERT_MSG(
        config.m_max_frame_delta_seconds >= config.m_fixed_tick_seconds,
        "max_frame_delta_seconds must be >= fixed_tick_seconds");
    MORDOR_ASSERT_MSG(callbacks.m_simulate, "callbacks.m_simulate must not be null");
    MORDOR_ASSERT_MSG(callbacks.m_render, "callbacks.m_render must not be null");

    using clock = std::chrono::steady_clock;

    auto last = clock::now();
    const auto start = last;

    double accumulator = 0.0;
    LoopStats stats{};

    while (std::chrono::duration<double>(clock::now() - start).count() < config.m_max_run_seconds)
    {
        if (callbacks.m_should_continue && !callbacks.m_should_continue())
        {
            break;
        }

        const auto now = clock::now();
        double frame_delta = std::chrono::duration<double>(now - last).count();
        last = now;

        if (frame_delta > config.m_max_frame_delta_seconds)
        {
            frame_delta = config.m_max_frame_delta_seconds;
        }

        accumulator += frame_delta;

        while (accumulator >= config.m_fixed_tick_seconds)
        {
            callbacks.m_simulate(config.m_fixed_tick_seconds);
            accumulator -= config.m_fixed_tick_seconds;
            ++stats.m_simulation_steps;
        }

        const double raw_alpha = accumulator / config.m_fixed_tick_seconds;
        const double alpha = raw_alpha < 0.0 ? 0.0 : (raw_alpha > 1.0 ? 1.0 : raw_alpha);
        callbacks.m_render(alpha);
        ++stats.m_render_frames;

        // Yield to the scheduler without enforcing fixed pacing in this reusable loop.
        std::this_thread::yield();
    }

    return stats;
}

} // namespace mordor
