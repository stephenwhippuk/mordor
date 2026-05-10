#include "mordor/log.hpp"
#include "mordor/assert.hpp"
#include "mordor/profiler.hpp"
#include "mordor/main_loop.hpp"
#include "mordor/renderer.hpp"
#include "mordor/map.hpp"
#include "mordor/world_mesh.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/fog_of_war.hpp"
#include "mordor/hearing.hpp"
#include "mordor/hud_surfaces.hpp"
#include "mordor/inventory_pipeline.hpp"
#include "mordor/perception_debug.hpp"
#include "mordor/party_commands.hpp"
#include "mordor/scene.hpp"
#include "mordor/visibility.hpp"

#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#ifndef MORDOR_BUILD_TYPE
    #define MORDOR_BUILD_TYPE "Unknown"
#endif

struct DemoWorld
{
    uint64_t m_tick_count{0};
    double m_player_move_cooldown_seconds{0.0};
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

        const bool blocked = mordor::dungeon_tile_blocks_physical(*tile);
        const float r = blocked ? 0.62F : 0.18F;
        const float g = blocked ? 0.20F : 0.43F;
        const float b = blocked ? 0.20F : 0.24F;
        const float width = node->m_world_bounds.m_max.m_x - node->m_world_bounds.m_min.m_x;
        const float height = node->m_world_bounds.m_max.m_y - node->m_world_bounds.m_min.m_y;
        const float center_x = node->m_world_bounds.m_min.m_x + width * 0.5F;
        const float center_y = node->m_world_bounds.m_min.m_y + height * 0.5F;

        tiles.push_back(mordor::DebugTile{
            .m_x = center_x,
            .m_y = center_y,
            .m_width = width - 2.0F,
            .m_height = height - 2.0F,
            .m_r = r,
            .m_g = g,
            .m_b = b,
        });
    }

    return tiles;
}

mordor::TileCoord clamp_tile_to_grid(const mordor::OccupancyGrid& grid, mordor::TileCoord tile)
{
    const int max_col = std::max(0, grid.m_width - 1);
    const int max_row = std::max(0, grid.m_height - 1);
    return mordor::TileCoord{
        .m_col = std::clamp(tile.m_col, 0, max_col),
        .m_row = std::clamp(tile.m_row, 0, max_row),
    };
}

mordor::TileCoord nearest_unblocked_tile(
    const mordor::OccupancyGrid& grid,
    mordor::TileCoord preferred)
{
    preferred = clamp_tile_to_grid(grid, preferred);
    if (!mordor::is_tile_blocked(grid, preferred.m_col, preferred.m_row))
    {
        return preferred;
    }

    const int max_radius = std::max(grid.m_width, grid.m_height);
    for (int radius = 1; radius <= max_radius; ++radius)
    {
        const int min_col = preferred.m_col - radius;
        const int max_col = preferred.m_col + radius;
        const int min_row = preferred.m_row - radius;
        const int max_row = preferred.m_row + radius;

        for (int row = min_row; row <= max_row; ++row)
        {
            for (int col = min_col; col <= max_col; ++col)
            {
                if (col != min_col && col != max_col && row != min_row && row != max_row)
                {
                    continue;
                }

                if (!mordor::in_occupancy_bounds(grid, col, row))
                {
                    continue;
                }

                if (!mordor::is_tile_blocked(grid, col, row))
                {
                    return mordor::TileCoord{.m_col = col, .m_row = row};
                }
            }
        }
    }

    return preferred;
}

mordor::Float3 screen_to_world_point(
    const mordor::CameraState& camera,
    int viewport_width,
    int viewport_height,
    float screen_x,
    float screen_y,
    float world_z)
{
    const float dx = screen_x - (static_cast<float>(viewport_width) * 0.5F);
    const float dy = screen_y - (static_cast<float>(viewport_height) * 0.5F);

    const float c = std::cos(camera.m_rotation_radians);
    const float s = std::sin(camera.m_rotation_radians);

    const float scaled_x = (dx * c) + (dy * s);
    const float scaled_y = (-dx * s) + (dy * c);

    const float inv_zoom = camera.m_zoom > 0.0F ? (1.0F / camera.m_zoom) : 1.0F;

    return mordor::Float3{
        .m_x = camera.m_x + (scaled_x * inv_zoom),
        .m_y = camera.m_y + (scaled_y * inv_zoom),
        .m_z = world_z,
    };
}

std::size_t count_fog_visible_tiles(const mordor::FogOfWarState& fog)
{
    std::size_t count = 0;
    for (uint8_t value : fog.m_visible)
    {
        if (value != 0U)
        {
            ++count;
        }
    }
    return count;
}

std::size_t count_fog_explored_tiles(const mordor::FogOfWarState& fog)
{
    std::size_t count = 0;
    for (uint8_t value : fog.m_explored)
    {
        if (value != 0U)
        {
            ++count;
        }
    }
    return count;
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

bool mark_player_spawn_tile(mordor::DungeonMap& map, int& out_col, int& out_row)
{
    for (mordor::DungeonTile& tile : map.m_tiles)
    {
        if (!mordor::dungeon_tile_blocks_physical(tile) && tile.m_symbol == '.')
        {
            out_col = tile.m_col;
            out_row = tile.m_row;
            return true;
        }
    }

    for (mordor::DungeonTile& tile : map.m_tiles)
    {
        if (!mordor::dungeon_tile_blocks_physical(tile))
        {
            out_col = tile.m_col;
            out_row = tile.m_row;
            return true;
        }
    }

    return false;
}

mordor::Float3 tile_center_world(mordor::TileCoord tile)
{
    return mordor::Float3{
        .m_x = (static_cast<float>(tile.m_col) + 0.5F) * mordor::k_scene_tile_world_size,
        .m_y = (static_cast<float>(tile.m_row) + 0.5F) * mordor::k_scene_tile_world_size,
        .m_z = 0.0F,
    };
}

mordor::Float3 camera_eye_world(const mordor::CameraState& camera)
{
    const float yaw = camera.m_rotation_radians;
    const float cos_pitch = std::cos(camera.m_pitch_radians);
    const float sin_pitch = std::sin(camera.m_pitch_radians);

    return mordor::Float3{
        .m_x = camera.m_x + std::sin(yaw) * cos_pitch * camera.m_distance,
        .m_y = camera.m_y + std::cos(yaw) * cos_pitch * camera.m_distance,
        .m_z = sin_pitch * camera.m_distance,
    };
}

bool try_move_player_marker(
    mordor::Scene& scene,
    mordor::SceneNodeId player_marker_node_id,
    const mordor::OccupancyGrid& occupancy,
    const mordor::WallCollisionOctree& wall_collision_octree,
    mordor::TileCoord& in_out_player_tile,
    int delta_col,
    int delta_row)
{
    if (delta_col == 0 && delta_row == 0)
    {
        return false;
    }

    const mordor::TileCoord destination = clamp_tile_to_grid(
        occupancy,
        mordor::TileCoord{
            .m_col = in_out_player_tile.m_col + delta_col,
            .m_row = in_out_player_tile.m_row + delta_row,
        });

    if (destination.m_col == in_out_player_tile.m_col && destination.m_row == in_out_player_tile.m_row)
    {
        return false;
    }

    if (mordor::is_tile_blocked(occupancy, destination.m_col, destination.m_row))
    {
        return false;
    }

    const mordor::SceneNode* player_node = mordor::find_scene_node(scene, player_marker_node_id);
    if (player_node == nullptr)
    {
        return false;
    }

    const mordor::Float3 destination_world = tile_center_world(destination);
    const mordor::Bounds3 candidate_bounds{
        .m_min = mordor::Float3{
            .m_x = destination_world.m_x + player_node->m_local_bounds.m_min.m_x,
            .m_y = destination_world.m_y + player_node->m_local_bounds.m_min.m_y,
            .m_z = destination_world.m_z + player_node->m_local_bounds.m_min.m_z,
        },
        .m_max = mordor::Float3{
            .m_x = destination_world.m_x + player_node->m_local_bounds.m_max.m_x,
            .m_y = destination_world.m_y + player_node->m_local_bounds.m_max.m_y,
            .m_z = destination_world.m_z + player_node->m_local_bounds.m_max.m_z,
        },
    };
    if (mordor::wall_collision_octree_overlaps_bounds(wall_collision_octree, candidate_bounds))
    {
        return false;
    }

    const uint32_t scene_blocking_flags =
        mordor::scene_node_category_bits(mordor::SceneNodeCategory::BlocksMovement);
    if (mordor::any_scene_node_blocks_bounds(
            scene,
            candidate_bounds,
            player_marker_node_id,
            scene_blocking_flags))
    {
        return false;
    }

    if (!mordor::update_scene_node_world_position(scene, player_marker_node_id, destination_world))
    {
        return false;
    }

    in_out_player_tile = destination;
    return true;
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

    const mordor::DungeonGenerationConfig generation_config{
        .m_width = 48,
        .m_height = 36,
        .m_room_count = 12,
        .m_min_room_size = 5,
        .m_max_room_size = 10,
        .m_seed = 1337U,
        .m_enable_key_switch_constraints = true,
        .m_enable_prefab_insertion = true,
        .m_prefab_attempt_count = 2,
    };

    mordor::DungeonValidationReport validation_report{};
    if (mordor::generate_room_corridor_dungeon_map(generation_config, handcrafted_map)
        && mordor::validate_generated_dungeon_map(handcrafted_map, validation_report))
    {
        loaded_map_path = "generated(seed=1337)";
        MORDOR_LOG_INFO(
            "Generated dungeon map source='{}' size={}x{} walkable={} reachable={} constraints={} prefabs={}",
            loaded_map_path,
            handcrafted_map.m_width,
            handcrafted_map.m_height,
            validation_report.m_walkable_tile_count,
            validation_report.m_reachable_with_unlocks,
            handcrafted_map.m_generated_constraints.size(),
            handcrafted_map.m_prefab_placements.size());
    }
    else
    {
        MORDOR_LOG_WARN("Generated dungeon failed validation; falling back to handcrafted map");
        for (const std::string& issue : validation_report.m_issues)
        {
            MORDOR_LOG_WARN("generation_issue: {}", issue);
        }

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
    }

    int player_col = -1;
    int player_row = -1;
    if (mark_player_spawn_tile(handcrafted_map, player_col, player_row))
    {
        MORDOR_LOG_INFO("Selected player spawn tile at ({}, {})", player_col, player_row);
    }
    else
    {
        MORDOR_LOG_WARN("Could not place player marker tile (no walkable tiles found)");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    mordor::TileCoord active_player_tile{.m_col = player_col, .m_row = player_row};

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

    const mordor::Float3 initial_player_world = tile_center_world(active_player_tile);
    const mordor::SceneNodeId player_marker_node_id =
        mordor::add_runtime_marker_node(world_scene, 'A', initial_player_world);
    if (player_marker_node_id == mordor::k_invalid_scene_node_id)
    {
        MORDOR_LOG_CRITICAL("Failed to create player marker scene node");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    const mordor::CameraState initial_camera = renderer.camera_state();
    const mordor::Float3 initial_camera_eye = camera_eye_world(initial_camera);
    const mordor::WorldMesh world_mesh = mordor::build_world_mesh(
        world_scene,
        handcrafted_map,
        initial_camera_eye.m_x,
        initial_camera_eye.m_y,
        initial_player_world.m_x,
        initial_player_world.m_y);
    if (world_mesh.m_vertices.empty() || world_mesh.m_indices.empty())
    {
        MORDOR_LOG_CRITICAL("Failed to build world mesh from scene/map");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }
    MORDOR_LOG_INFO(
        "Built world mesh vertices={} indices={}",
        world_mesh.m_vertices.size(),
        world_mesh.m_indices.size());
    renderer.load_world_mesh(world_mesh);
    renderer.set_occlusion_target_world(initial_player_world.m_x, initial_player_world.m_y);


    mordor::OccupancyGrid occupancy_grid{};
    if (!mordor::build_occupancy_grid_from_map(handcrafted_map, occupancy_grid))
    {
        MORDOR_LOG_CRITICAL("Failed to build occupancy grid from handcrafted map");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    mordor::WallCollisionOctree wall_collision_octree{};
    if (mordor::build_wall_collision_octree(handcrafted_map, wall_collision_octree))
    {
        MORDOR_LOG_INFO(
            "Built wall collision octree surfaces={} nodes={}",
            wall_collision_octree.m_surfaces.size(),
            wall_collision_octree.m_nodes.size());
    }
    else
    {
        MORDOR_LOG_WARN("Wall collision octree build returned empty structure");
    }

    mordor::FogOfWarState fog_of_war{};
    if (!mordor::build_fog_of_war_state(occupancy_grid, fog_of_war))
    {
        MORDOR_LOG_CRITICAL("Failed to build fog-of-war state from occupancy grid");
        renderer.shutdown();
        mordor::log::shutdown();
        return 1;
    }

    MORDOR_LOG_INFO(
        "Perception debug legend: fog_visible=gold fog_explored=slate los_path=cyan los_blocker=red hearing_path_audible=green hearing_path_muffled=gray hearing_listener=teal hearing_source=orange");

    const mordor::SceneDebugMetrics scene_metrics =
        mordor::collect_scene_debug_metrics(world_scene);
    MORDOR_LOG_INFO(
        "Scene index metrics: cells={} leaf_cells={} max_depth={} indexed_nodes={}",
        scene_metrics.m_cell_count,
        scene_metrics.m_leaf_cell_count,
        scene_metrics.m_max_depth,
        scene_metrics.m_indexed_node_count);

    const std::vector<mordor::DebugTile> base_debug_map =
        build_debug_tiles_from_scene(world_scene, handcrafted_map);
    std::vector<mordor::DebugTile> debug_map = base_debug_map;
    std::vector<mordor::ScreenOverlayRect> hud_overlay_rects{};

    mordor::PartySelectionState party_selection{};
    party_selection.m_selected_actor_ids = {1U, 2U};
    party_selection.m_primary_actor_id = 1U;

    mordor::PartyCommandQueue party_queue{};
    party_queue.m_intents.push_back(mordor::PartyCommandIntent{
        .m_issuer_entity_id = 1U,
        .m_type = mordor::PartyCommandType::MoveToTile,
        .m_target_tile = mordor::TileCoord{.m_col = 4, .m_row = 4},
    });

    mordor::AbilityExecutionQueue ability_queue{};
    ability_queue.m_requests.push_back(mordor::AbilityRequest{
        .m_issuer_entity_id = 1U,
        .m_ability_id = 100U,
        .m_target_entity_id = 3U,
    });

    mordor::ItemUseQueue item_use_queue{};
    item_use_queue.m_intents.push_back(mordor::ItemUseIntent{
        .m_issuer_entity_id = 1U,
        .m_item_id = 200U,
        .m_has_target_tile = true,
        .m_target_tile = mordor::TileCoord{.m_col = 6, .m_row = 3},
        .m_consumes_on_use = true,
    });

    const std::vector<mordor::InventoryItemEntry> demo_inventory_items{
        mordor::InventoryItemEntry{.m_owner_entity_id = 1U, .m_item_id = 200U, .m_quantity = 2U},
        mordor::InventoryItemEntry{.m_owner_entity_id = 1U, .m_item_id = 201U, .m_quantity = 1U},
        mordor::InventoryItemEntry{.m_owner_entity_id = 2U, .m_item_id = 250U, .m_quantity = 3U},
    };

    mordor::LoopConfig config{};
    config.m_fixed_tick_seconds = 1.0 / 60.0;
    config.m_max_frame_delta_seconds = 0.25;
    config.m_max_run_seconds = 120.0;

    mordor::LoopCallbacks callbacks{};
    callbacks.m_simulate =
        [&world,
         &renderer,
         &world_scene,
         &occupancy_grid,
         &wall_collision_octree,
         player_marker_node_id,
         &active_player_tile,
         &fog_of_war,
         &base_debug_map,
         &debug_map,
         &hud_overlay_rects,
         &party_selection,
         &party_queue,
         &ability_queue,
         &item_use_queue,
         &demo_inventory_items](double dt) {
        MORDOR_PROFILE_SCOPE("simulate");
        ++world.m_tick_count;

        renderer.update_camera_controls(dt);

        world.m_player_move_cooldown_seconds = std::max(0.0, world.m_player_move_cooldown_seconds - dt);

        int player_move_col = 0;
        int player_move_row = 0;
        if (renderer.is_input_action_active(mordor::InputAction::MovePlayerLeft))
        {
            player_move_col -= 1;
        }
        if (renderer.is_input_action_active(mordor::InputAction::MovePlayerRight))
        {
            player_move_col += 1;
        }
        if (renderer.is_input_action_active(mordor::InputAction::MovePlayerUp))
        {
            player_move_row += 1;
        }
        if (renderer.is_input_action_active(mordor::InputAction::MovePlayerDown))
        {
            player_move_row -= 1;
        }

        // Keep movement cardinal while bootstrapping controls.
        if (player_move_col != 0 && player_move_row != 0)
        {
            player_move_row = 0;
        }

        if (world.m_player_move_cooldown_seconds <= 0.0)
        {
            if (try_move_player_marker(
                    world_scene,
                    player_marker_node_id,
                    occupancy_grid,
                    wall_collision_octree,
                    active_player_tile,
                    player_move_col,
                    player_move_row))
            {
                world.m_player_move_cooldown_seconds = 0.12;
                MORDOR_LOG_DEBUG(
                    "player_move tick={} tile=({}, {})",
                    world.m_tick_count,
                    active_player_tile.m_col,
                    active_player_tile.m_row);
            }
        }

        const mordor::Float3 player_anchor_world = tile_center_world(active_player_tile);
        renderer.set_occlusion_target_world(player_anchor_world.m_x, player_anchor_world.m_y);

        if (world.m_tick_count % 60 == 0)
        {
            const mordor::CameraState camera = renderer.camera_state();
            MORDOR_LOG_DEBUG(
                "simulate tick={} dt={} camera=({}, {}, zoom={}, rot={}, pitch={}, dist={})",
                world.m_tick_count,
                dt,
                camera.m_x,
                camera.m_y,
                camera.m_zoom,
                camera.m_rotation_radians,
                camera.m_pitch_radians,
                camera.m_distance);
        }

        if (world.m_tick_count % 120 == 0)
        {
            constexpr float k_sample_half_extent = 96.0F;

            const auto framebuffer_size = renderer.framebuffer_size();
            if (framebuffer_size.m_width > 0 && framebuffer_size.m_height > 0)
            {
                // Use actual mouse position for picking query
                int mouse_x = 0;
                int mouse_y = 0;
                renderer.mouse_position(mouse_x, mouse_y);

                const float sample_screen_x = static_cast<float>(mouse_x);
                const float sample_screen_y = static_cast<float>(mouse_y);

                const mordor::CameraState camera = renderer.camera_state();
                const mordor::Float3 sample_world = screen_to_world_point(
                    camera,
                    framebuffer_size.m_width,
                    framebuffer_size.m_height,
                    sample_screen_x,
                    sample_screen_y,
                    0.0F);

                const uint32_t pickable_flags =
                    mordor::scene_node_category_bits(mordor::SceneNodeCategory::StaticGeometry)
                    | mordor::scene_node_category_bits(mordor::SceneNodeCategory::Pickable);

                const std::vector<mordor::SceneNodeId> point_hits =
                    mordor::query_scene_point(world_scene, sample_world, pickable_flags);
                const mordor::SceneNodeId picked_id =
                    mordor::pick_scene_node_at_point(world_scene, sample_world, pickable_flags);

                const mordor::Bounds3 neighborhood{
                    .m_min = mordor::Float3{
                        .m_x = sample_world.m_x - k_sample_half_extent,
                        .m_y = sample_world.m_y - k_sample_half_extent,
                        .m_z = -1.0F,
                    },
                    .m_max = mordor::Float3{
                        .m_x = sample_world.m_x + k_sample_half_extent,
                        .m_y = sample_world.m_y + k_sample_half_extent,
                        .m_z = 1.0F,
                    },
                };
                const std::vector<mordor::SceneNodeId> neighborhood_hits =
                    mordor::query_scene_bounds(world_scene, neighborhood);

                MORDOR_LOG_DEBUG(
                    "scene_query tick={} framebuffer=({}x{}) mouse=({}, {}) world_point=({}, {}, {}) point_hits={} picked={} neighborhood_hits={}",
                    world.m_tick_count,
                    framebuffer_size.m_width,
                    framebuffer_size.m_height,
                    mouse_x,
                    mouse_y,
                    sample_world.m_x,
                    sample_world.m_y,
                    sample_world.m_z,
                    point_hits.size(),
                    picked_id,
                    neighborhood_hits.size());

                const mordor::TileCoord observer_tile = clamp_tile_to_grid(
                    occupancy_grid,
                    mordor::TileCoord{
                        .m_col = static_cast<int>(std::floor(sample_world.m_x / mordor::k_scene_tile_world_size)),
                        .m_row = static_cast<int>(std::floor(sample_world.m_y / mordor::k_scene_tile_world_size)),
                    });

                const int target_distance_tiles = std::max(occupancy_grid.m_width, occupancy_grid.m_height);
                const mordor::TileCoord preferred_los_target = clamp_tile_to_grid(
                    occupancy_grid,
                    mordor::TileCoord{
                        .m_col = observer_tile.m_col
                            + static_cast<int>(std::round(std::cos(camera.m_rotation_radians) * static_cast<float>(target_distance_tiles))),
                        .m_row = observer_tile.m_row
                            + static_cast<int>(std::round(std::sin(camera.m_rotation_radians) * static_cast<float>(target_distance_tiles))),
                    });
                const mordor::TileCoord los_target =
                    nearest_unblocked_tile(occupancy_grid, preferred_los_target);

                const mordor::LineOfSightResult los_trace =
                    mordor::trace_line_of_sight(occupancy_grid, observer_tile, los_target);

                const mordor::HearingEvent hearing_event{
                    .m_kind = mordor::HearingEventKind::Interaction,
                    .m_source_tile = clamp_tile_to_grid(
                        occupancy_grid,
                        mordor::TileCoord{.m_col = occupancy_grid.m_width - 1, .m_row = occupancy_grid.m_height - 1}),
                    .m_loudness = 1.0F,
                    .m_max_range_tiles = static_cast<float>(std::max(occupancy_grid.m_width, occupancy_grid.m_height)),
                };
                const mordor::HearingListener hearing_listener{
                    .m_listener_tile = observer_tile,
                    .m_forward = mordor::Float3{
                        .m_x = std::cos(camera.m_rotation_radians),
                        .m_y = std::sin(camera.m_rotation_radians),
                        .m_z = 0.0F,
                    },
                    .m_rear_gain = 0.35F,
                    .m_detection_threshold = 0.05F,
                };
                const mordor::HearingResult hearing_result =
                    mordor::evaluate_hearing_event(occupancy_grid, hearing_event, hearing_listener);

                const std::vector<mordor::FogOfWarObserver> fog_observers{
                    mordor::FogOfWarObserver{.m_tile = observer_tile, .m_vision_range_tiles = 6},
                };
                if (mordor::refresh_fog_of_war(fog_of_war, occupancy_grid, fog_observers))
                {
                    debug_map = base_debug_map;
                    mordor::append_fog_of_war_debug_tiles(fog_of_war, debug_map);
                    mordor::append_line_of_sight_debug_tiles(los_trace, debug_map);
                    const std::vector<mordor::TileCoord> hearing_path =
                        mordor::build_tile_line(observer_tile, hearing_event.m_source_tile);
                    mordor::append_hearing_debug_tiles(
                        observer_tile,
                        hearing_event,
                        hearing_result,
                        hearing_path,
                        debug_map);

                    MORDOR_LOG_DEBUG(
                        "perception_overlay tick={} observer=({}, {}) los_target=({}, {}) los_blocked={} los_blocker=({}, {}) hearing_source=({}, {}) hearing_audible={} hearing_occluders={} fog_visible={} fog_explored={}",
                        world.m_tick_count,
                        observer_tile.m_col,
                        observer_tile.m_row,
                        los_target.m_col,
                        los_target.m_row,
                        los_trace.m_hit_blocker,
                        los_trace.m_first_blocking_tile.m_col,
                        los_trace.m_first_blocking_tile.m_row,
                        hearing_event.m_source_tile.m_col,
                        hearing_event.m_source_tile.m_row,
                        hearing_result.m_audible,
                        hearing_result.m_occluding_blocker_count,
                        count_fog_visible_tiles(fog_of_war),
                        count_fog_explored_tiles(fog_of_war));
                }

                const mordor::HudPanelMetrics hud_metrics = mordor::build_hud_panel_metrics(
                    party_selection,
                    party_queue,
                    ability_queue,
                    item_use_queue,
                    demo_inventory_items,
                    party_selection.m_primary_actor_id);
                hud_overlay_rects = mordor::build_baseline_hud_surfaces(
                    hud_metrics,
                    framebuffer_size.m_width,
                    framebuffer_size.m_height);
            }
        }
    };
    callbacks.m_render = [&renderer, &world_scene, &handcrafted_map, &hud_overlay_rects, &world](double alpha) {
        MORDOR_PROFILE_SCOPE("render");
        (void)alpha;

        renderer.begin_frame();
        renderer.draw_world(world_scene, handcrafted_map);
        renderer.draw_screen_overlay(hud_overlay_rects);
        renderer.end_frame();

        // P5-05: Log render metrics every 120 ticks.
        if (world.m_tick_count % 120 == 0)
        {
            const mordor::RenderMetrics metrics = renderer.render_metrics();
            MORDOR_LOG_DEBUG(
                "render_metrics tick={} total_indices={} visible_indices={} culled_indices={} "
                "visible_nodes={} culled_nodes={} culling_rate={}%",
                world.m_tick_count,
                metrics.m_total_indices,
                metrics.m_visible_indices,
                metrics.m_culled_indices,
                metrics.m_frustum_visible_nodes,
                metrics.m_frustum_culled_nodes,
                (metrics.m_total_indices > 0)
                    ? static_cast<int>((100.0 * metrics.m_culled_indices) / metrics.m_total_indices)
                    : 0);
        }
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
