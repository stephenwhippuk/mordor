#include "mordor/log.hpp"
#include "mordor/assert.hpp"
#include "mordor/profiler.hpp"
#include "mordor/main_loop.hpp"

struct DemoWorld
{
    uint64_t tick_count{0};
};

int main()
{
    mordor::log::init();

    MORDOR_LOG_INFO("Mordor engine bootstrap OK");
    MORDOR_LOG_DEBUG("Build type: " MORDOR_BUILD_TYPE);

    {
        MORDOR_PROFILE_SCOPE("bootstrap");
        MORDOR_ASSERT_MSG(1 + 1 == 2, "basic sanity check");
    }

    DemoWorld world{};

    mordor::LoopConfig config{};
    config.fixed_tick_seconds = 1.0 / 60.0;
    config.max_frame_delta_seconds = 0.25;
    config.max_run_seconds = 2.0;

    auto stats = mordor::run_main_loop(
        config,
        mordor::LoopCallbacks{
            .simulate = [&world](double dt) {
                MORDOR_PROFILE_SCOPE("simulate");
                ++world.tick_count;

                if (world.tick_count % 60 == 0)
                {
                    MORDOR_LOG_DEBUG("simulate tick={} dt={}", world.tick_count, dt);
                }
            },
            .render = [](double alpha) {
                MORDOR_PROFILE_SCOPE("render");

                // Placeholder until real renderer is connected in P1-02.
                if (alpha > 0.95)
                {
                    MORDOR_LOG_TRACE("render alpha={}", alpha);
                }
            },
        });

    MORDOR_LOG_INFO(
        "loop complete: simulation_steps={} render_frames={}",
        stats.simulation_steps,
        stats.render_frames);

    mordor::profiler::for_each([](std::string_view name, uint64_t ns) {
        MORDOR_LOG_DEBUG("Profile [{}]: {} ns", name, ns);
    });

    mordor::log::shutdown();
    return 0;
}
