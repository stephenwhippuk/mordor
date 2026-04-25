#include "mordor/log.hpp"
#include "mordor/assert.hpp"
#include "mordor/profiler.hpp"
#include "mordor/main_loop.hpp"
#include "mordor/renderer.hpp"

#include <cstdint>
#include <vector>

#ifndef MODOR_BUILD_TYPE
    #define MORDOR_BUILD_TYPE "unknown"
#endif

struct DemoWorld
{
    uint64_t m_tick_count{0};
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
    mordor::Renderer renderer{};

    if (!renderer.init(mordor::RendererConfig{}))
    {
        MORDOR_LOG_CRITICAL("Renderer bootstrap failed");
        mordor::log::shutdown();
        return 1;
    }

    std::vector<mordor::DebugTile> debug_map{
        {.m_x = 40.0F, .m_y = 40.0F, .m_width = 180.0F, .m_height = 100.0F},
        {.m_x = 240.0F, .m_y = 120.0F, .m_width = 140.0F, .m_height = 80.0F},
        {.m_x = 390.0F, .m_y = 220.0F, .m_width = 200.0F, .m_height = 120.0F},
    };

    mordor::LoopConfig config{};
    config.m_fixed_tick_seconds = 1.0 / 60.0;
    config.m_max_frame_delta_seconds = 0.25;
    config.m_max_run_seconds = 120.0;

    mordor::LoopCallbacks callbacks{};
    callbacks.m_simulate = [&world](double dt) {
        MORDOR_PROFILE_SCOPE("simulate");
        ++world.m_tick_count;

        if (world.m_tick_count % 60 == 0)
        {
            MORDOR_LOG_DEBUG("simulate tick={} dt={}", world.m_tick_count, dt);
        }
    };
    callbacks.m_render = [&renderer, &debug_map](double alpha) {
        MORDOR_PROFILE_SCOPE("render");
        (void)alpha;

        renderer.begin_frame();
        renderer.draw_debug_map(debug_map);
        renderer.end_frame();
    };
    callbacks.m_should_continue = [&renderer]() {
        return !renderer.should_close();
    };

    auto stats = mordor::run_main_loop(config, callbacks);

    MORDOR_LOG_INFO(
        "loop complete: simulation_steps={} render_frames={}",
        stats.m_simulation_steps,
        stats.m_render_frames);

    mordor::profiler::for_each([](std::string_view name, uint64_t ns) {
        MORDOR_LOG_DEBUG("Profile [{}]: {} ns", name, ns);
    });

    renderer.shutdown();

    mordor::log::shutdown();
    return 0;
}
