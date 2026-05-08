#include "mordor/components.hpp"
#include "mordor/interactions.hpp"
#include "mordor/key_switch.hpp"
#include "mordor/map.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/scene.hpp"
#include "mordor/visibility.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

using namespace mordor;

bool g_all_ok = true;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        g_all_ok = false;
        std::cerr << "[FAIL] " << message << '\n';
    }
}

DungeonMap build_test_map()
{
    DungeonMap map{};
    map.m_width = 3;
    map.m_height = 2;
    map.m_tiles = {
        DungeonTile{.m_col = 0, .m_row = 0, .m_blocks_movement = false, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 0, .m_blocks_movement = true, .m_symbol = '#'},
        DungeonTile{.m_col = 2, .m_row = 0, .m_blocks_movement = false, .m_symbol = '.'},
        DungeonTile{.m_col = 0, .m_row = 1, .m_blocks_movement = false, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 1, .m_blocks_movement = false, .m_symbol = '.'},
        DungeonTile{.m_col = 2, .m_row = 1, .m_blocks_movement = false, .m_symbol = '.'},
    };
    return map;
}

void test_interaction_state_machines()
{
    InteractableComponent door{};
    door.m_entity_id = 1U;
    door.m_kind = InteractableKind::Door;
    door.m_state = InteractableState::Closed;

    check(apply_interaction_event(door, InteractionEvent::Use), "door should open on use");
    check(door.m_state == InteractableState::Open, "door state should be open");

    check(apply_interaction_event(door, InteractionEvent::Close), "door should close");
    check(door.m_state == InteractableState::Closed, "door state should be closed");

    check(apply_interaction_event(door, InteractionEvent::Lock), "door should lock");
    check(door.m_state == InteractableState::Locked, "door state should be locked");

    check(!apply_interaction_event(door, InteractionEvent::Open), "locked door should not open directly");

    InteractableComponent trap{};
    trap.m_entity_id = 2U;
    trap.m_kind = InteractableKind::Trap;
    trap.m_state = InteractableState::Armed;

    check(apply_interaction_event(trap, InteractionEvent::Trigger), "armed trap should trigger");
    check(trap.m_state == InteractableState::Triggered, "trap state should be triggered");

    check(apply_interaction_event(trap, InteractionEvent::Reset), "triggered trap should reset");
    check(trap.m_state == InteractableState::Armed, "trap state should return to armed");
}

void test_key_and_switch_logic()
{
    InteractableComponent locked_door{};
    locked_door.m_entity_id = 10U;
    locked_door.m_kind = InteractableKind::Door;
    locked_door.m_state = InteractableState::Locked;
    locked_door.m_required_key_id = 42U;

    KeyRingComponent keyring{};
    keyring.m_actor_entity_id = 100U;

    check(!try_unlock_interactable(keyring, locked_door), "unlock should fail without key");
    check(grant_key_to_actor(keyring, 42U), "granting key should succeed");
    check(try_unlock_interactable(keyring, locked_door), "unlock should succeed with key");
    check(locked_door.m_state == InteractableState::Closed, "door should be closed after unlock");

    InteractableComponent sw{};
    sw.m_entity_id = 20U;
    sw.m_kind = InteractableKind::Switch;
    sw.m_state = InteractableState::Closed;

    InteractableComponent target_door{};
    target_door.m_entity_id = 21U;
    target_door.m_kind = InteractableKind::Door;
    target_door.m_state = InteractableState::Closed;

    std::vector<InteractableComponent> interactables{target_door};
    std::vector<SwitchLinkModel> links{SwitchLinkModel{
        .m_switch_entity_id = 20U,
        .m_target_entity_id = 21U,
        .m_on_activate_event = InteractionEvent::Open,
        .m_on_deactivate_event = InteractionEvent::Close,
    }};

    check(toggle_switch_and_apply_links(sw, links, interactables), "switch toggle should succeed");
    check(sw.m_state == InteractableState::Open, "switch should be open after toggle");
    check(interactables[0].m_state == InteractableState::Open, "linked target should open");
}

void test_occupancy_rules()
{
    const DungeonMap map = build_test_map();

    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(map, grid), "occupancy grid build should succeed");
    check(is_tile_blocked(grid, 1, 0), "wall tile should be statically blocked");
    check(!is_tile_blocked(grid, 0, 0), "floor tile should not be statically blocked");

    const EntityId actor_id = 500U;
    ActorComponent actor{};
    actor.m_entity_id = actor_id;

    TransformComponent actor_tf{};
    actor_tf.m_entity_id = actor_id;
    actor_tf.m_local_offset = Float3{.m_x = 2.0F * k_scene_tile_world_size, .m_y = 0.0F, .m_z = 0.0F};

    std::vector<ActorComponent> actors{actor};
    std::vector<TransformComponent> transforms{actor_tf};

    check(apply_actor_occupancy_to_grid(grid, actors, transforms), "actor occupancy pass should change grid");
    check(is_tile_occupied(grid, 2, 0), "actor tile should be occupied");
    check(!is_tile_walkable(grid, 2, 0), "occupied tile should not be walkable");

    clear_dynamic_occupancy(grid);

    InteractableComponent blocking_door{};
    blocking_door.m_entity_id = 600U;
    blocking_door.m_kind = InteractableKind::Door;
    blocking_door.m_state = InteractableState::Closed;
    blocking_door.m_blocks_movement_when_active = true;

    InteractableComponent non_blocking_switch{};
    non_blocking_switch.m_entity_id = 601U;
    non_blocking_switch.m_kind = InteractableKind::Switch;
    non_blocking_switch.m_state = InteractableState::Open;
    non_blocking_switch.m_blocks_movement_when_active = false;

    TransformComponent tf_door{};
    tf_door.m_entity_id = 600U;
    tf_door.m_local_offset = Float3{.m_x = k_scene_tile_world_size, .m_y = k_scene_tile_world_size, .m_z = 0.0F};

    TransformComponent tf_switch{};
    tf_switch.m_entity_id = 601U;
    tf_switch.m_local_offset = Float3{.m_x = k_scene_tile_world_size, .m_y = k_scene_tile_world_size, .m_z = 0.0F};

    std::vector<InteractableComponent> interactables{blocking_door, non_blocking_switch};
    std::vector<TransformComponent> interactable_tfs{tf_door, tf_switch};

    check(
        apply_interactable_blocking_to_grid(grid, interactables, interactable_tfs),
        "interactable blocking pass should change grid");
    check(
        is_tile_blocked(grid, 1, 1),
        "shared interactable tile should remain blocked when one interactable blocks");
}

void test_line_of_sight_rules()
{
    const DungeonMap map = build_test_map();
    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(map, grid), "occupancy grid build for los should succeed");

    check(
        has_line_of_sight(grid, TileCoord{.m_col = 0, .m_row = 1}, TileCoord{.m_col = 2, .m_row = 1}),
        "clear horizontal path should have line of sight");

    const LineOfSightResult blocked_result =
        trace_line_of_sight(grid, TileCoord{.m_col = 0, .m_row = 0}, TileCoord{.m_col = 2, .m_row = 0});
    check(!blocked_result.m_has_line_of_sight, "wall between origin and target should block los");
    check(blocked_result.m_hit_blocker, "blocked los trace should report blocker");
    check(
        blocked_result.m_first_blocking_tile.m_col == 1 && blocked_result.m_first_blocking_tile.m_row == 0,
        "blocked los trace should identify the correct blocking tile");

    const LineOfSightResult blocked_target_result =
        trace_line_of_sight(grid, TileCoord{.m_col = 0, .m_row = 0}, TileCoord{.m_col = 1, .m_row = 0});
    check(!blocked_target_result.m_has_line_of_sight, "blocked target tile should not have line of sight");
    check(blocked_target_result.m_hit_blocker, "blocked target should report a blocker");
    check(
        blocked_target_result.m_first_blocking_tile.m_col == 1
            && blocked_target_result.m_first_blocking_tile.m_row == 0,
        "blocked target should report itself as the blocking tile");

    OccupancyGrid corner_grid{};
    corner_grid.m_width = 2;
    corner_grid.m_height = 2;
    corner_grid.m_static_blocking.assign(4, 0U);
    corner_grid.m_dynamic_blocking.assign(4, 0U);
    corner_grid.m_occupants.assign(4, k_invalid_entity_id);
    check(set_tile_dynamic_blocked(corner_grid, 1, 0, true), "corner wall setup should block east tile");
    check(set_tile_dynamic_blocked(corner_grid, 0, 1, true), "corner wall setup should block south tile");

    const LineOfSightResult corner_result =
        trace_line_of_sight(corner_grid, TileCoord{.m_col = 0, .m_row = 0}, TileCoord{.m_col = 1, .m_row = 1});
    check(!corner_result.m_has_line_of_sight, "los should not pass through a fully blocked diagonal corner");
    check(corner_result.m_hit_blocker, "blocked corner trace should report a blocker");
    check(
        corner_result.m_first_blocking_tile.m_col == 1 && corner_result.m_first_blocking_tile.m_row == 0,
        "blocked corner should report one of the blocking edge tiles");
}

} // namespace

int main()
{
    try
    {
        test_interaction_state_machines();
        test_key_and_switch_logic();
        test_occupancy_rules();
        test_line_of_sight_rules();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] unexpected exception: " << ex.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "[FAIL] unknown exception" << '\n';
        return EXIT_FAILURE;
    }

    if (!g_all_ok)
    {
        return EXIT_FAILURE;
    }

    std::cout << "[PASS] mordor_unit_tests" << '\n';
    return EXIT_SUCCESS;
}
