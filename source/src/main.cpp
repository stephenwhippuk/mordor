#include "mordor/log.hpp"
#include "mordor/assert.hpp"
#include "mordor/profiler.hpp"
#include "mordor/main_loop.hpp"
#include "mordor/renderer.hpp"
#include "mordor/map.hpp"
#include "mordor/scene.hpp"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#ifndef MORDOR_BUILD_TYPE
    #define MORDOR_BUILD_TYPE "Unknown"
#endif

struct DemoWorld
{
    uint64_t m_tick_count{0};
};

namespace {

void add_candidate_path(std::vector<std::filesystem::path>& out_paths, const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }

    for (const std::filesystem::path& existing : out_paths)
    {
        if (existing == path)
        {
            return;
        }
    }

    out_paths.push_back(path);
}

std::vector<std::filesystem::path> map_candidate_paths(int argc, char** argv)
{
    std::vector<std::filesystem::path> paths{};

    if (const char* env_path = std::getenv("MORDOR_HANDCRAFTED_MAP"); env_path != nullptr)
    {
        add_candidate_path(paths, std::filesystem::path(env_path));
    }

    const std::filesystem::path map_relative = std::filesystem::path("source") / "data" / "maps"
                                             / "handcrafted_dungeon.map";

    add_candidate_path(paths, map_relative);

    const std::filesystem::path cwd = std::filesystem::current_path();
    add_candidate_path(paths, cwd / map_relative);
    add_candidate_path(paths, cwd / ".." / map_relative);
    add_candidate_path(paths, cwd / ".." / ".." / map_relative);

    if (argc > 0 && argv != nullptr && argv[0] != nullptr)
    {
        const std::filesystem::path executable_path(argv[0]);
        const std::filesystem::path executable_dir = executable_path.parent_path();
        add_candidate_path(paths, executable_dir / ".." / ".." / map_relative);
        add_candidate_path(paths, executable_dir / ".." / map_relative);
    }

    return paths;
}

std::vector<mordor::DebugTile> build_debug_tiles_from_scene(
    const mordor::Scene& scene,
    const mordor::DungeonMap& map)
{
    std::vector<mordor::DebugTile> tiles{};
    const std::vector<mordor::SceneNodeId> visible_nodes =
        mordor::query_scene_bounds(scene, scene.m_spatial_index.m_root_bounds);
    tiles.reserve(visible_nodes.size());

    const uint32_t renderable_static_flags =
        mordor::scene_node_category_bits(mordor::SceneNodeCategory::StaticGeometry)
        | mordor::scene_node_category_bits(mordor::SceneNodeCategory::Renderable);

    for (mordor::SceneNodeId node_id : visible_nodes)
    {
        const mordor::SceneNode* node = mordor::find_scene_node(scene, node_id);
        if (node == nullptr || !mordor::scene_node_has_flags(*node, renderable_static_flags))
        {
            continue;
        }

        const mordor::DungeonTile* tile = nullptr;
        if (node->m_payload_index >= 0)
        {
            const std::size_t tile_index = static_cast<std::size_t>(node->m_payload_index);
            if (tile_index < map.m_tiles.size())
            {
                tile = &map.m_tiles[tile_index];
            }
        }

        if (tile == nullptr)
        {
            continue;
        }

        const bool blocked = tile->m_blocks_movement;
        const float r = blocked ? 0.62F : 0.18F;
        const float g = blocked ? 0.20F : 0.43F;
        const float b = blocked ? 0.20F : 0.24F;
        const float width = node->m_world_bounds.m_max.m_x - node->m_world_bounds.m_min.m_x;
        const float height = node->m_world_bounds.m_max.m_y - node->m_world_bounds.m_min.m_y;

        tiles.push_back(mordor::DebugTile{
            .m_x = node->m_world_position.m_x,
            .m_y = node->m_world_position.m_y,
            .m_width = width - 2.0F,
            .m_height = height - 2.0F,
            .m_r = r,
            .m_g = g,
            .m_b = b,
        });
    }

    return tiles;
}

bool try_load_handcrafted_map(
    mordor::DungeonMap& out_map,
    std::string& out_loaded_path,
    int argc,
    char** argv)
{
    const std::vector<std::filesystem::path> candidates = map_candidate_paths(argc, argv);
    for (const std::filesystem::path& path : candidates)
    {
        const std::string path_string = path.string();
        MORDOR_LOG_DEBUG("Attempting handcrafted map load from '{}'", path_string);
        if (mordor::load_handcrafted_dungeon_map(path_string, out_map))
        {
            out_loaded_path = path_string;
            return true;
        }
    }

    MORDOR_LOG_ERROR("Handcrafted map load failed after trying {} paths", candidates.size());
    return false;
}

} // namespace

int main(int argc, char** argv)
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

    mordor::DungeonMap handcrafted_map{};
    std::string loaded_map_path{};
    if (!try_load_handcrafted_map(handcrafted_map, loaded_map_path, argc, argv))
    {
        MORDOR_LOG_CRITICAL("Failed to load handcrafted dungeon map from expected paths");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    MORDOR_LOG_INFO(
        "Loaded handcrafted dungeon map path='{}' size={}x{}",
        loaded_map_path,
        handcrafted_map.m_width,
        handcrafted_map.m_height);

    mordor::Scene world_scene{};
    if (!mordor::build_scene_from_dungeon_map(handcrafted_map, world_scene))
    {
        MORDOR_LOG_CRITICAL("Failed to build runtime scene from handcrafted dungeon map");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    MORDOR_LOG_INFO(
        "Built runtime scene nodes={} indexed_nodes={} cells={}",
        world_scene.m_nodes.size(),
        world_scene.m_spatial_index.m_indexed_node_count,
        world_scene.m_spatial_index.m_cells.size());

    std::vector<mordor::DebugTile> debug_map = build_debug_tiles_from_scene(world_scene, handcrafted_map);

    mordor::LoopConfig config{};
    config.m_fixed_tick_seconds = 1.0 / 60.0;
    config.m_max_frame_delta_seconds = 0.25;
    config.m_max_run_seconds = 120.0;

    mordor::LoopCallbacks callbacks{};
    callbacks.m_simulate = [&world, &renderer](double dt) {
        MORDOR_PROFILE_SCOPE("simulate");
        ++world.m_tick_count;

        renderer.update_camera_controls(dt);

        if (world.m_tick_count % 60 == 0)
        {
            const mordor::CameraState camera = renderer.camera_state();
            MORDOR_LOG_DEBUG(
                "simulate tick={} dt={} camera=({}, {}, zoom={}, rot={})",
                world.m_tick_count,
                dt,
                camera.m_x,
                camera.m_y,
                camera.m_zoom,
                camera.m_rotation_radians);
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
