#include "mordor/ability_pipeline.hpp"
#include "mordor/components.hpp"
#include "mordor/fog_of_war.hpp"
#include "mordor/hearing.hpp"
#include "mordor/hud_surfaces.hpp"
#include "mordor/inventory_pipeline.hpp"
#include "mordor/interactions.hpp"
#include "mordor/key_switch.hpp"
#include "mordor/map.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/party_commands.hpp"
#include "mordor/perception_debug.hpp"
#include "mordor/scene.hpp"
#include "mordor/visibility.hpp"
#include "mordor/world_mesh.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
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

void check_near(float expected, float actual, float epsilon, const std::string& message)
{
    check(std::fabs(expected - actual) <= epsilon, message);
}

DungeonMap build_test_map()
{
    DungeonMap map{};
    map.m_width = 3;
    map.m_height = 2;
    map.m_tiles = {
        DungeonTile{.m_col = 0, .m_row = 0, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 0, .m_collision_mask = k_tile_collision_solid, .m_symbol = '#'},
        DungeonTile{.m_col = 2, .m_row = 0, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 0, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 2, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
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

void test_generated_constraint_runtime_binding_rules()
{
    DungeonMap map = build_test_map();
    map.m_generated_constraints.push_back(DungeonMap::DoorConstraint{
        .m_key_id = 9001U,
        .m_door_col = 1,
        .m_door_row = 0,
        .m_key_col = 0,
        .m_key_row = 1,
        .m_switch_col = 2,
        .m_switch_row = 1,
    });

    std::vector<GeneratedConstraintBinding> bindings{};
    std::vector<InteractableComponent> interactables{};
    std::vector<SwitchLinkModel> switch_links{};
    EntityId next_entity_id = k_invalid_entity_id;

    check(
        append_generated_constraint_runtime_models(
            map,
            10000U,
            bindings,
            interactables,
            switch_links,
            next_entity_id),
        "generated constraint runtime model build should succeed");
    check(bindings.size() == 1U, "generated constraint runtime model should emit one binding");
    check(interactables.size() == 2U, "generated constraint runtime model should emit door and switch interactables");
    check(switch_links.size() == 1U, "generated constraint runtime model should emit one switch link");
    check(next_entity_id == 10002U, "generated constraint runtime model should advance entity id cursor");

    InteractableComponent* door = nullptr;
    InteractableComponent* sw = nullptr;
    for (InteractableComponent& component : interactables)
    {
        if (component.m_kind == InteractableKind::Door)
        {
            door = &component;
        }
        else if (component.m_kind == InteractableKind::Switch)
        {
            sw = &component;
        }
    }

    check(door != nullptr, "generated constraint runtime model should include a door interactable");
    check(sw != nullptr, "generated constraint runtime model should include a switch interactable");
    if (door == nullptr || sw == nullptr)
    {
        return;
    }

    check(door->m_state == InteractableState::Locked, "generated door should start locked");
    check(door->m_required_key_id == 9001U, "generated door should carry required key id");

    KeyRingComponent keyring{};
    keyring.m_actor_entity_id = 77U;
    check(!try_unlock_interactable(keyring, *door), "generated door should not unlock before key is granted");
    check(grant_key_to_actor(keyring, 9001U), "generated key id should be grantable to keyring");
    check(try_unlock_interactable(keyring, *door), "generated door should unlock once required key is granted");
    check(door->m_state == InteractableState::Closed, "generated door should be closed after key-based unlock");

    // Relock door and verify switch link unlock path from generated switch bindings.
    check(apply_interaction_event(*door, InteractionEvent::Lock), "generated door should be lockable for switch-link test");
    check(toggle_switch_and_apply_links(*sw, switch_links, interactables), "generated switch link should toggle and apply");
    check(door->m_state == InteractableState::Closed, "generated switch link should unlock door to closed state");

    DungeonMap map_for_collision_override = build_test_map();
    map_for_collision_override.m_width = 3;
    map_for_collision_override.m_height = 2;
    map_for_collision_override.m_tiles = {
        DungeonTile{.m_col = 0, .m_row = 0, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 0, .m_collision_mask = k_tile_collision_solid, .m_symbol = 'D'},
        DungeonTile{.m_col = 2, .m_row = 0, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 0, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 1, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
        DungeonTile{.m_col = 2, .m_row = 1, .m_collision_mask = k_tile_collision_none, .m_symbol = '.'},
    };
    map_for_collision_override.m_generated_constraints.push_back(DungeonMap::DoorConstraint{
        .m_key_id = 700U,
        .m_door_col = 1,
        .m_door_row = 0,
        .m_key_col = 0,
        .m_key_row = 1,
        .m_switch_col = 2,
        .m_switch_row = 1,
    });

    check(
        apply_generated_constraint_door_collision_overrides(map_for_collision_override),
        "generated door collision override should succeed");
    check(
        !dungeon_tile_blocks_physical(map_for_collision_override.m_tiles[1]),
        "generated door tile should no longer be statically blocking after override");

    OccupancyGrid collision_grid{};
    check(
        build_occupancy_grid_from_map(map_for_collision_override, collision_grid),
        "occupancy build should succeed after generated door collision override");
    check(
        !is_tile_blocked(collision_grid, 1, 0),
        "generated door tile should not be statically blocked in occupancy after override");

    std::vector<GeneratedConstraintBinding> dynamic_bindings{};
    std::vector<InteractableComponent> dynamic_interactables{};
    std::vector<SwitchLinkModel> dynamic_switch_links{};
    EntityId dynamic_next_id = k_invalid_entity_id;
    check(
        append_generated_constraint_runtime_models(
            map_for_collision_override,
            20000U,
            dynamic_bindings,
            dynamic_interactables,
            dynamic_switch_links,
            dynamic_next_id),
        "runtime models should build after generated door collision override");

    std::vector<TransformComponent> dynamic_transforms{};
    dynamic_transforms.push_back(TransformComponent{
        .m_entity_id = dynamic_bindings[0].m_door_entity_id,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = 1.5F * k_scene_tile_world_size, .m_y = 0.5F * k_scene_tile_world_size, .m_z = 0.0F},
    });
    dynamic_transforms.push_back(TransformComponent{
        .m_entity_id = dynamic_bindings[0].m_switch_entity_id,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = 2.5F * k_scene_tile_world_size, .m_y = 1.5F * k_scene_tile_world_size, .m_z = 0.0F},
    });

    check(
        apply_interactable_blocking_to_grid(collision_grid, dynamic_interactables, dynamic_transforms),
        "dynamic interactable blocking should apply for generated door runtime model");
    check(
        is_tile_blocked(collision_grid, 1, 0),
        "generated door tile should be dynamically blocked while door is locked/closed");
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

void test_directional_hearing_rules()
{
    const DungeonMap map = build_test_map();
    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(map, grid), "occupancy grid build for hearing should succeed");

    const HearingEvent event_front{
        .m_kind = HearingEventKind::Movement,
        .m_source_tile = TileCoord{.m_col = 2, .m_row = 1},
        .m_loudness = 1.0F,
        .m_max_range_tiles = 4.0F,
    };

    const HearingListener facing_source{
        .m_listener_tile = TileCoord{.m_col = 0, .m_row = 1},
        .m_forward = Float3{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F},
        .m_rear_gain = 0.35F,
        .m_detection_threshold = 0.05F,
    };
    const HearingListener facing_away{
        .m_listener_tile = TileCoord{.m_col = 0, .m_row = 1},
        .m_forward = Float3{.m_x = -1.0F, .m_y = 0.0F, .m_z = 0.0F},
        .m_rear_gain = 0.35F,
        .m_detection_threshold = 0.05F,
    };

    const HearingResult front_result = evaluate_hearing_event(grid, event_front, facing_source);
    const HearingResult back_result = evaluate_hearing_event(grid, event_front, facing_away);
    check(front_result.m_audible, "front hearing result should be audible");
    check(back_result.m_audible, "rear hearing result should remain audible with baseline rear gain");
    check(
        front_result.m_perceived_loudness > back_result.m_perceived_loudness,
        "facing source should increase perceived loudness");
    check(front_result.m_directional_alignment > back_result.m_directional_alignment, "alignment should reflect facing");
    check_near(2.0F, front_result.m_distance_tiles, 0.0001F, "distance should match tile separation");

    const HearingEvent blocked_event{
        .m_kind = HearingEventKind::Interaction,
        .m_source_tile = TileCoord{.m_col = 2, .m_row = 0},
        .m_loudness = 1.0F,
        .m_max_range_tiles = 4.0F,
    };
    const HearingListener blocked_listener{
        .m_listener_tile = TileCoord{.m_col = 0, .m_row = 0},
        .m_forward = Float3{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F},
        .m_rear_gain = 0.35F,
        .m_detection_threshold = 0.05F,
    };
    const HearingResult blocked_result = evaluate_hearing_event(grid, blocked_event, blocked_listener);
    check(blocked_result.m_occluding_blocker_count >= 1, "blocked hearing path should report occluders");

    const HearingEvent clear_event_same_distance{
        .m_kind = HearingEventKind::Interaction,
        .m_source_tile = TileCoord{.m_col = 2, .m_row = 1},
        .m_loudness = 1.0F,
        .m_max_range_tiles = 4.0F,
    };
    const HearingListener clear_listener{
        .m_listener_tile = TileCoord{.m_col = 0, .m_row = 1},
        .m_forward = Float3{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F},
        .m_rear_gain = 0.35F,
        .m_detection_threshold = 0.05F,
    };
    const HearingResult clear_result = evaluate_hearing_event(grid, clear_event_same_distance, clear_listener);
    check(
        blocked_result.m_perceived_loudness < clear_result.m_perceived_loudness,
        "occluded hearing should attenuate perceived loudness");

    const HearingEvent out_of_range_event{
        .m_kind = HearingEventKind::Ambient,
        .m_source_tile = TileCoord{.m_col = 2, .m_row = 1},
        .m_loudness = 1.0F,
        .m_max_range_tiles = 1.0F,
    };
    const HearingResult out_of_range_result = evaluate_hearing_event(grid, out_of_range_event, facing_source);
    check(!out_of_range_result.m_audible, "out-of-range hearing event should not be audible");
}

void test_fog_of_war_rules()
{
    const DungeonMap map = build_test_map();
    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(map, grid), "occupancy grid build for fog should succeed");

    FogOfWarState fog{};
    check(build_fog_of_war_state(grid, fog), "fog state build should succeed");

    std::vector<FogOfWarObserver> observers{FogOfWarObserver{
        .m_tile = TileCoord{.m_col = 0, .m_row = 0},
        .m_vision_range_tiles = 3,
    }};
    check(refresh_fog_of_war(fog, grid, observers), "fog refresh should succeed");

    check(is_fog_visible(fog, 0, 0), "observer tile should be visible");
    check(is_fog_visible(fog, 0, 1), "near clear tile should be visible");
    check(!is_fog_visible(fog, 2, 0), "wall-occluded tile should not be visible");
    check(is_fog_explored(fog, 0, 1), "visible tile should be marked explored");

    observers[0].m_tile = TileCoord{.m_col = 2, .m_row = 1};
    observers[0].m_vision_range_tiles = 1;
    check(refresh_fog_of_war(fog, grid, observers), "fog refresh after move should succeed");
    check(!is_fog_visible(fog, 0, 1), "previously visible tile should not remain visible after observer move");
    check(is_fog_explored(fog, 0, 1), "previously seen tile should remain explored after visibility clears");
}

void test_perception_debug_overlay_rules()
{
    const DungeonMap map = build_test_map();
    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(map, grid), "occupancy grid build for debug overlays should succeed");

    FogOfWarState fog{};
    check(build_fog_of_war_state(grid, fog), "fog state build for debug overlays should succeed");
    const std::vector<FogOfWarObserver> observers{FogOfWarObserver{
        .m_tile = TileCoord{.m_col = 0, .m_row = 0},
        .m_vision_range_tiles = 3,
    }};
    check(refresh_fog_of_war(fog, grid, observers), "fog refresh for debug overlays should succeed");

    std::vector<DebugTile> overlays{};
    const std::size_t fog_count = append_fog_of_war_debug_tiles(fog, overlays);
    check(fog_count > 0, "fog debug overlay should add tiles");

    const LineOfSightResult los =
        trace_line_of_sight(grid, TileCoord{.m_col = 0, .m_row = 0}, TileCoord{.m_col = 2, .m_row = 0});
    const std::size_t los_count = append_line_of_sight_debug_tiles(los, overlays);
    check(los_count >= los.m_traversed_tiles.size(), "los overlay should include traced path tiles");

    const std::vector<TileCoord> line = build_tile_line(TileCoord{.m_col = 0, .m_row = 0}, TileCoord{.m_col = 2, .m_row = 1});
    check(!line.empty(), "tile-line helper should generate line points");
    check(line.front().m_col == 0 && line.front().m_row == 0, "tile-line should include start");
    check(line.back().m_col == 2 && line.back().m_row == 1, "tile-line should include end");

    const HearingEvent event{
        .m_kind = HearingEventKind::Movement,
        .m_source_tile = TileCoord{.m_col = 2, .m_row = 1},
        .m_loudness = 1.0F,
        .m_max_range_tiles = 4.0F,
    };
    const HearingListener listener{
        .m_listener_tile = TileCoord{.m_col = 0, .m_row = 0},
        .m_forward = Float3{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F},
        .m_rear_gain = 0.35F,
        .m_detection_threshold = 0.05F,
    };
    const HearingResult hearing = evaluate_hearing_event(grid, event, listener);
    const std::size_t hearing_count =
        append_hearing_debug_tiles(listener.m_listener_tile, event, hearing, line, overlays);
    check(hearing_count >= line.size() + 2U, "hearing overlay should include path plus listener/source markers");
}

void test_party_selection_and_command_rules()
{
    RuntimeComponents components{};

    const EntityId friendly_a = allocate_entity(components);
    const EntityId friendly_b = allocate_entity(components);
    const EntityId hostile = allocate_entity(components);

    components.m_actors.push_back(ActorComponent{
        .m_entity_id = friendly_a,
        .m_archetype_id = 1U,
        .m_max_action_points = 2U,
        .m_current_action_points = 2U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Friendly,
    });
    components.m_actors.push_back(ActorComponent{
        .m_entity_id = friendly_b,
        .m_archetype_id = 2U,
        .m_max_action_points = 1U,
        .m_current_action_points = 1U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Friendly,
    });
    components.m_actors.push_back(ActorComponent{
        .m_entity_id = hostile,
        .m_archetype_id = 3U,
        .m_max_action_points = 2U,
        .m_current_action_points = 2U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Hostile,
    });

    PartySelectionState selection{};
    check(select_party_actor(components, friendly_a, false, selection), "friendly actor should be selectable");
    check(is_actor_selected(selection, friendly_a), "selected actor should be tracked");
    check(selection.m_primary_actor_id == friendly_a, "primary actor should match selected actor");

    check(!select_party_actor(components, hostile, true, selection), "hostile actor should not be selectable");
    check(selection.m_selected_actor_ids.size() == 1U, "failed hostile selection should not mutate selection");

    check(select_party_actor(components, friendly_b, true, selection), "additive friendly selection should succeed");
    check(selection.m_selected_actor_ids.size() == 2U, "additive selection should include both actors");
    check(selection.m_primary_actor_id == friendly_b, "last selected actor should become primary");

    check(deselect_party_actor(selection, friendly_b), "deselecting selected actor should succeed");
    check(selection.m_primary_actor_id == friendly_a, "primary should fall back to remaining selected actor");

    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(build_test_map(), grid), "occupancy build should succeed for command checks");

    PartyCommandQueue queue{};

    const PartyCommandIntent blocked_move{
        .m_issuer_entity_id = friendly_a,
        .m_type = PartyCommandType::MoveToTile,
        .m_target_tile = TileCoord{.m_col = 1, .m_row = 0},
    };
    const PartyCommandIssueResult blocked_move_result =
        issue_party_command(components, grid, selection, blocked_move, queue);
    check(
        blocked_move_result.m_status == PartyCommandIssueStatus::RejectedBlockedTarget,
        "blocked movement command should be rejected");

    check(set_tile_occupant(grid, 2, 1, friendly_b), "occupied move test setup should place occupant");

    const PartyCommandIntent occupied_move{
        .m_issuer_entity_id = friendly_a,
        .m_type = PartyCommandType::MoveToTile,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    const PartyCommandIssueResult occupied_move_result =
        issue_party_command(components, grid, selection, occupied_move, queue);
    check(
        occupied_move_result.m_status == PartyCommandIssueStatus::RejectedOccupiedTarget,
        "occupied movement command should be rejected");
    check(clear_tile_occupant(grid, 2, 1, friendly_b), "occupied move test cleanup should clear occupant");

    const PartyCommandIntent clear_move{
        .m_issuer_entity_id = friendly_a,
        .m_type = PartyCommandType::MoveToTile,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    const PartyCommandIssueResult clear_move_result =
        issue_party_command(components, grid, selection, clear_move, queue);
    check(clear_move_result.m_status == PartyCommandIssueStatus::Accepted, "clear movement command should be accepted");
    check(queue.m_intents.size() == 1U, "accepted command should be queued");

    const PartyCommandIntent missing_entity_interact{
        .m_issuer_entity_id = friendly_a,
        .m_type = PartyCommandType::InteractWithEntity,
        .m_target_entity_id = 9999U,
    };
    const PartyCommandIssueResult missing_entity_interact_result =
        issue_party_command(components, grid, selection, missing_entity_interact, queue);
    check(
        missing_entity_interact_result.m_status == PartyCommandIssueStatus::RejectedTargetEntityNotFound,
        "interact command should reject non-existent target entity");

    const PartyCommandIntent wrong_issuer{
        .m_issuer_entity_id = hostile,
        .m_type = PartyCommandType::Wait,
    };
    const PartyCommandIssueResult wrong_issuer_result =
        issue_party_command(components, grid, selection, wrong_issuer, queue);
    check(
        wrong_issuer_result.m_status == PartyCommandIssueStatus::RejectedIssuerNotSelected,
        "issuer not in selection should be rejected");

    clear_party_selection(selection);
    const PartyCommandIssueResult no_selection_result =
        issue_party_command(components, grid, selection, clear_move, queue);
    check(
        no_selection_result.m_status == PartyCommandIssueStatus::RejectedNoSelection,
        "issuing with no selection should be rejected");
}

void test_ability_pipeline_rules()
{
    RuntimeComponents components{};

    const EntityId caster = allocate_entity(components);
    const EntityId target_actor = allocate_entity(components);

    components.m_actors.push_back(ActorComponent{
        .m_entity_id = caster,
        .m_archetype_id = 10U,
        .m_max_action_points = 3U,
        .m_current_action_points = 3U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Friendly,
    });
    components.m_actors.push_back(ActorComponent{
        .m_entity_id = target_actor,
        .m_archetype_id = 11U,
        .m_max_action_points = 2U,
        .m_current_action_points = 2U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Hostile,
    });

    components.m_transforms.push_back(TransformComponent{
        .m_entity_id = caster,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = 0.0F, .m_y = 0.0F, .m_z = 0.0F},
    });
    components.m_transforms.push_back(TransformComponent{
        .m_entity_id = target_actor,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = k_scene_tile_world_size, .m_y = 0.0F, .m_z = 0.0F},
    });

    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(build_test_map(), grid), "ability test occupancy build should succeed");
    check(set_tile_occupant(grid, 1, 1, target_actor), "ability test should place occupant on target tile");

    const std::vector<AbilitySpec> specs{
        AbilitySpec{
            .m_ability_id = 100U,
            .m_target_kind = AbilityTargetKind::Entity,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
        },
        AbilitySpec{
            .m_ability_id = 101U,
            .m_target_kind = AbilityTargetKind::Tile,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
        },
        AbilitySpec{
            .m_ability_id = 102U,
            .m_target_kind = AbilityTargetKind::Tile,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = true,
        },
        AbilitySpec{
            .m_ability_id = 103U,
            .m_target_kind = AbilityTargetKind::None,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 0U,
            .m_allow_empty_tile_target = false,
        },
        AbilitySpec{
            .m_ability_id = 104U,
            .m_target_kind = AbilityTargetKind::Entity,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 0U,
            .m_allow_empty_tile_target = false,
        },
        AbilitySpec{
            .m_ability_id = 105U,
            .m_target_kind = AbilityTargetKind::Entity,
            .m_action_point_cost = 5U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
        },
    };

    AbilityExecutionQueue queue{};

    const AbilityRequest unknown_ability{
        .m_issuer_entity_id = caster,
        .m_ability_id = 999U,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_ability_request(components, grid, specs, unknown_ability, queue).m_status
            == AbilityIssueStatus::RejectedUnknownAbility,
        "unknown ability id should be rejected");

    const AbilityRequest missing_target_entity{
        .m_issuer_entity_id = caster,
        .m_ability_id = 100U,
        .m_target_entity_id = 5555U,
    };
    check(
        issue_ability_request(components, grid, specs, missing_target_entity, queue).m_status
            == AbilityIssueStatus::RejectedTargetEntityNotFound,
        "entity-targeted ability should reject missing target entity");

    const AbilityRequest mixed_entity_and_tile_target{
        .m_issuer_entity_id = caster,
        .m_ability_id = 100U,
        .m_target_entity_id = target_actor,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_ability_request(components, grid, specs, mixed_entity_and_tile_target, queue).m_status
            == AbilityIssueStatus::RejectedInvalidTarget,
        "entity-targeted ability should reject mixed entity and tile target fields");

    const AbilityRequest entity_ok{
        .m_issuer_entity_id = caster,
        .m_ability_id = 100U,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_ability_request(components, grid, specs, entity_ok, queue).m_status
            == AbilityIssueStatus::Accepted,
        "entity-targeted ability should accept existing in-range target");

    const AbilityRequest tile_needs_occupied_but_empty{
        .m_issuer_entity_id = caster,
        .m_ability_id = 101U,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_ability_request(components, grid, specs, tile_needs_occupied_but_empty, queue).m_status
            == AbilityIssueStatus::RejectedInvalidTarget,
        "tile-target ability without empty-tile support should reject empty tiles");

    const AbilityRequest mixed_tile_and_entity_target{
        .m_issuer_entity_id = caster,
        .m_ability_id = 101U,
        .m_target_entity_id = target_actor,
        .m_target_tile = TileCoord{.m_col = 1, .m_row = 1},
    };
    check(
        issue_ability_request(components, grid, specs, mixed_tile_and_entity_target, queue).m_status
            == AbilityIssueStatus::RejectedInvalidTarget,
        "tile-target ability should reject mixed entity and tile target fields");

    const AbilityRequest tile_needs_occupied_ok{
        .m_issuer_entity_id = caster,
        .m_ability_id = 101U,
        .m_target_tile = TileCoord{.m_col = 1, .m_row = 1},
    };
    check(
        issue_ability_request(components, grid, specs, tile_needs_occupied_ok, queue).m_status
            == AbilityIssueStatus::Accepted,
        "tile-target ability should accept occupied tile when empty-tile targeting is disabled");

    const AbilityRequest tile_empty_allowed{
        .m_issuer_entity_id = caster,
        .m_ability_id = 102U,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_ability_request(components, grid, specs, tile_empty_allowed, queue).m_status
            == AbilityIssueStatus::Accepted,
        "tile-target ability with empty-tile support should accept empty tiles");

    const AbilityRequest none_with_target{
        .m_issuer_entity_id = caster,
        .m_ability_id = 103U,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_ability_request(components, grid, specs, none_with_target, queue).m_status
            == AbilityIssueStatus::RejectedInvalidTarget,
        "none-target ability should reject non-empty target fields");

    const AbilityRequest none_ok{
        .m_issuer_entity_id = caster,
        .m_ability_id = 103U,
    };
    check(
        issue_ability_request(components, grid, specs, none_ok, queue).m_status
            == AbilityIssueStatus::Accepted,
        "none-target ability should accept default empty target fields");

    const AbilityRequest out_of_range_entity{
        .m_issuer_entity_id = caster,
        .m_ability_id = 104U,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_ability_request(components, grid, specs, out_of_range_entity, queue).m_status
            == AbilityIssueStatus::RejectedOutOfRange,
        "entity-targeted ability should reject out-of-range target");

    const AbilityRequest insufficient_ap{
        .m_issuer_entity_id = caster,
        .m_ability_id = 105U,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_ability_request(components, grid, specs, insufficient_ap, queue).m_status
            == AbilityIssueStatus::RejectedInsufficientActionPoints,
        "ability should reject issuer with insufficient action points");

    AbilityExecutionQueue full_queue{};
    full_queue.m_max_size = 1U;
    const AbilityRequest queue_seed{
        .m_issuer_entity_id = caster,
        .m_ability_id = 100U,
        .m_target_entity_id = target_actor,
    };
    const AbilityRequest queue_overflow{
        .m_issuer_entity_id = caster,
        .m_ability_id = 100U,
        .m_target_entity_id = target_actor,
    };

    check(
        issue_ability_request(components, grid, specs, queue_seed, full_queue).m_status
            == AbilityIssueStatus::Accepted,
        "queue seed request should be accepted");
    check(
        issue_ability_request(components, grid, specs, queue_overflow, full_queue).m_status
            == AbilityIssueStatus::RejectedQueueFull,
        "ability request should reject when execution queue is full");
}

void test_inventory_pipeline_rules()
{
    RuntimeComponents components{};

    const EntityId caster = allocate_entity(components);
    const EntityId target_actor = allocate_entity(components);

    components.m_actors.push_back(ActorComponent{
        .m_entity_id = caster,
        .m_archetype_id = 20U,
        .m_max_action_points = 3U,
        .m_current_action_points = 3U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Friendly,
    });
    components.m_actors.push_back(ActorComponent{
        .m_entity_id = target_actor,
        .m_archetype_id = 21U,
        .m_max_action_points = 2U,
        .m_current_action_points = 2U,
        .m_move_speed_units_per_second = 3.0F,
        .m_allegiance = Allegiance::Hostile,
    });

    components.m_inventories.push_back(InventoryComponent{
        .m_entity_id = caster,
        .m_capacity = 8U,
        .m_item_count = 2U,
    });

    components.m_transforms.push_back(TransformComponent{
        .m_entity_id = caster,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = 0.0F, .m_y = 0.0F, .m_z = 0.0F},
    });
    components.m_transforms.push_back(TransformComponent{
        .m_entity_id = target_actor,
        .m_scene_node_id = k_invalid_scene_node_id,
        .m_local_offset = Float3{.m_x = k_scene_tile_world_size, .m_y = 0.0F, .m_z = 0.0F},
    });

    OccupancyGrid grid{};
    check(build_occupancy_grid_from_map(build_test_map(), grid), "inventory test occupancy build should succeed");
    check(set_tile_occupant(grid, 1, 1, target_actor), "inventory test should place occupant on target tile");

    const std::vector<ItemSpec> item_specs{
        ItemSpec{
            .m_item_id = 200U,
            .m_target_kind = ItemTargetKind::Entity,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
            .m_consumes_on_use = true,
        },
        ItemSpec{
            .m_item_id = 201U,
            .m_target_kind = ItemTargetKind::Tile,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
            .m_consumes_on_use = false,
        },
        ItemSpec{
            .m_item_id = 202U,
            .m_target_kind = ItemTargetKind::Tile,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = true,
            .m_consumes_on_use = true,
        },
        ItemSpec{
            .m_item_id = 203U,
            .m_target_kind = ItemTargetKind::None,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 0U,
            .m_allow_empty_tile_target = false,
            .m_consumes_on_use = true,
        },
        ItemSpec{
            .m_item_id = 204U,
            .m_target_kind = ItemTargetKind::Entity,
            .m_action_point_cost = 5U,
            .m_max_range_tiles = 4U,
            .m_allow_empty_tile_target = false,
            .m_consumes_on_use = false,
        },
        ItemSpec{
            .m_item_id = 205U,
            .m_target_kind = ItemTargetKind::Entity,
            .m_action_point_cost = 1U,
            .m_max_range_tiles = 0U,
            .m_allow_empty_tile_target = false,
            .m_consumes_on_use = false,
        },
    };

    const std::vector<InventoryItemEntry> inventory_items{
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 200U, .m_quantity = 1U},
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 201U, .m_quantity = 1U},
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 202U, .m_quantity = 2U},
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 203U, .m_quantity = 1U},
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 204U, .m_quantity = 1U},
        InventoryItemEntry{.m_owner_entity_id = caster, .m_item_id = 205U, .m_quantity = 1U},
    };

    check(has_inventory_item(inventory_items, caster, 200U, 1U), "inventory lookup should find owned item");
    check(!has_inventory_item(inventory_items, caster, 200U, 2U), "inventory lookup should enforce quantity");

    ItemUseQueue queue{};

    const ItemUseRequest unknown_item{
        .m_issuer_entity_id = caster,
        .m_item_id = 999U,
        .m_has_target_entity = true,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, unknown_item, queue).m_status
            == ItemUseIssueStatus::RejectedUnknownItem,
        "unknown item should be rejected");

    const ItemUseRequest missing_target_entity{
        .m_issuer_entity_id = caster,
        .m_item_id = 200U,
        .m_has_target_entity = true,
        .m_target_entity_id = 9999U,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, missing_target_entity, queue).m_status
            == ItemUseIssueStatus::RejectedTargetEntityNotFound,
        "entity-target item should reject missing target entity");

    const ItemUseRequest mixed_entity_and_tile{
        .m_issuer_entity_id = caster,
        .m_item_id = 200U,
        .m_has_target_entity = true,
        .m_has_target_tile = true,
        .m_target_entity_id = target_actor,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, mixed_entity_and_tile, queue).m_status
            == ItemUseIssueStatus::RejectedInvalidTarget,
        "entity-target item should reject mixed entity and tile fields");

    const ItemUseRequest entity_ok{
        .m_issuer_entity_id = caster,
        .m_item_id = 200U,
        .m_has_target_entity = true,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, entity_ok, queue).m_status
            == ItemUseIssueStatus::Accepted,
        "entity-target item should accept existing in-range target");

    const ItemUseRequest tile_requires_occupied_but_empty{
        .m_issuer_entity_id = caster,
        .m_item_id = 201U,
        .m_has_target_tile = true,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, tile_requires_occupied_but_empty, queue)
            .m_status
            == ItemUseIssueStatus::RejectedInvalidTarget,
        "tile-target item without empty-tile support should reject empty tiles");

    const ItemUseRequest tile_requires_occupied_ok{
        .m_issuer_entity_id = caster,
        .m_item_id = 201U,
        .m_has_target_tile = true,
        .m_target_tile = TileCoord{.m_col = 1, .m_row = 1},
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, tile_requires_occupied_ok, queue).m_status
            == ItemUseIssueStatus::Accepted,
        "tile-target item should accept occupied tile when empty targeting is disabled");

    const ItemUseRequest tile_empty_allowed{
        .m_issuer_entity_id = caster,
        .m_item_id = 202U,
        .m_has_target_tile = true,
        .m_target_tile = TileCoord{.m_col = 2, .m_row = 1},
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, tile_empty_allowed, queue).m_status
            == ItemUseIssueStatus::Accepted,
        "tile-target item with empty-tile support should accept empty tiles");

    const ItemUseRequest none_with_target{
        .m_issuer_entity_id = caster,
        .m_item_id = 203U,
        .m_has_target_entity = true,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, none_with_target, queue).m_status
            == ItemUseIssueStatus::RejectedInvalidTarget,
        "none-target item should reject non-empty target fields");

    const ItemUseRequest none_ok{
        .m_issuer_entity_id = caster,
        .m_item_id = 203U,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, none_ok, queue).m_status
            == ItemUseIssueStatus::Accepted,
        "none-target item should accept default empty target fields");

    const ItemUseRequest insufficient_ap{
        .m_issuer_entity_id = caster,
        .m_item_id = 204U,
        .m_has_target_entity = true,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, insufficient_ap, queue).m_status
            == ItemUseIssueStatus::RejectedInsufficientActionPoints,
        "item use should reject issuer with insufficient action points");

    const ItemUseRequest out_of_range{
        .m_issuer_entity_id = caster,
        .m_item_id = 205U,
        .m_has_target_entity = true,
        .m_target_entity_id = target_actor,
    };
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, out_of_range, queue).m_status
            == ItemUseIssueStatus::RejectedOutOfRange,
        "item use should reject out-of-range target");

    ItemUseQueue full_queue{};
    full_queue.m_max_size = 1U;
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, entity_ok, full_queue).m_status
            == ItemUseIssueStatus::Accepted,
        "queue seed item use should be accepted");
    check(
        issue_item_use_request(components, grid, item_specs, inventory_items, entity_ok, full_queue).m_status
            == ItemUseIssueStatus::RejectedQueueFull,
        "item use should reject when queue is full");

    std::vector<InventoryItemEntry> zero_quantity_items = inventory_items;
    for (InventoryItemEntry& entry : zero_quantity_items)
    {
        if (entry.m_item_id == 200U)
        {
            entry.m_quantity = 0U;
            break;
        }
    }
    check(
        issue_item_use_request(components, grid, item_specs, zero_quantity_items, entity_ok, queue).m_status
            == ItemUseIssueStatus::RejectedInsufficientItemQuantity,
        "item use should reject entries with zero available quantity");
}

void test_hud_surface_rules()
{
    PartySelectionState selection{};
    selection.m_selected_actor_ids = {11U, 12U};
    selection.m_primary_actor_id = 11U;

    PartyCommandQueue party_queue{};
    party_queue.m_intents.push_back(PartyCommandIntent{.m_issuer_entity_id = 11U, .m_type = PartyCommandType::Wait});
    party_queue.m_intents.push_back(
        PartyCommandIntent{.m_issuer_entity_id = 12U, .m_type = PartyCommandType::MoveToTile, .m_target_tile = TileCoord{.m_col = 2, .m_row = 1}});

    AbilityExecutionQueue ability_queue{};
    ability_queue.m_requests.push_back(AbilityRequest{.m_issuer_entity_id = 11U, .m_ability_id = 100U});

    ItemUseQueue item_queue{};
    item_queue.m_intents.push_back(ItemUseIntent{.m_issuer_entity_id = 11U, .m_item_id = 200U, .m_consumes_on_use = true});
    item_queue.m_intents.push_back(ItemUseIntent{.m_issuer_entity_id = 11U, .m_item_id = 201U, .m_consumes_on_use = false});

    const std::vector<InventoryItemEntry> inventory_items{
        InventoryItemEntry{.m_owner_entity_id = 11U, .m_item_id = 200U, .m_quantity = 2U},
        InventoryItemEntry{.m_owner_entity_id = 11U, .m_item_id = 201U, .m_quantity = 1U},
        InventoryItemEntry{.m_owner_entity_id = 12U, .m_item_id = 300U, .m_quantity = 5U},
        InventoryItemEntry{.m_owner_entity_id = 11U, .m_item_id = 202U, .m_quantity = 0U},
    };

    const HudPanelMetrics metrics = build_hud_panel_metrics(
        selection,
        party_queue,
        ability_queue,
        item_queue,
        inventory_items,
        11U);

    check(metrics.m_selected_actor_count == 2U, "hud metrics should report selected actor count");
    check(metrics.m_has_primary_actor, "hud metrics should report a primary actor");
    check(metrics.m_party_queue_size == 2U, "hud metrics should report party queue size");
    check(metrics.m_ability_queue_size == 1U, "hud metrics should report ability queue size");
    check(metrics.m_item_queue_size == 2U, "hud metrics should report item queue size");
    check(metrics.m_inventory_entry_count == 2U, "hud metrics should ignore zero-quantity entries");
    check(metrics.m_inventory_total_quantity == 3U, "hud metrics should sum inventory quantities for owner");

    const std::vector<ScreenOverlayRect> rects = build_baseline_hud_surfaces(metrics, 1280, 720);
    check(rects.size() >= 20U, "hud surface builder should produce panels, bars, and label glyph rectangles");

    bool found_party_panel = false;
    bool found_inventory_panel = false;
    for (const ScreenOverlayRect& rect : rects)
    {
        check(rect.m_width > 0 && rect.m_height > 0, "hud rectangles should have positive size");
        check(rect.m_y >= 0 && rect.m_y < 720, "hud rectangles should remain within framebuffer y-bounds");
        if (rect.m_height == 92)
        {
            if (!found_inventory_panel)
            {
                found_inventory_panel = true;
            }
            else
            {
                found_party_panel = true;
            }
        }
    }
    check(found_party_panel && found_inventory_panel, "hud surfaces should include both baseline panels");

    const std::vector<ScreenOverlayRect> empty_rects = build_baseline_hud_surfaces(metrics, 0, 720);
    check(empty_rects.empty(), "hud surfaces should be empty for invalid framebuffer dimensions");
}

void test_world_mesh_generation_rules()
{
    const DungeonMap map = build_test_map();

    Scene scene{};
    check(build_scene_from_dungeon_map(map, scene), "scene build should succeed for world mesh tests");

    const WorldMesh mesh = build_world_mesh(scene, map, 0.0F, 0.0F, 0.0F, 0.0F);

    // build_test_map has 5 floor tiles and 1 wall tile.
    // floor emits 4 verts + 6 indices, wall emits 20 verts + 30 indices.
    const std::size_t expected_floor_count = 5U;
    const std::size_t expected_wall_count = 1U;
    const std::size_t expected_vertices = (expected_floor_count * 4U) + (expected_wall_count * 20U);
    const std::size_t expected_indices = (expected_floor_count * 6U) + (expected_wall_count * 30U);

    check(mesh.m_vertices.size() == expected_vertices, "world mesh vertex count should match floor+wall emission");
    check(mesh.m_indices.size() == expected_indices, "world mesh index count should match floor+wall emission");

    // Deterministic output: repeated builds should byte-match counts and index ordering.
    const WorldMesh mesh_again = build_world_mesh(scene, map, 0.0F, 0.0F, 0.0F, 0.0F);
    check(mesh_again.m_vertices.size() == mesh.m_vertices.size(), "world mesh build should be deterministic for vertices");
    check(mesh_again.m_indices.size() == mesh.m_indices.size(), "world mesh build should be deterministic for indices");
    check(mesh_again.m_indices == mesh.m_indices, "world mesh index ordering should be deterministic");

    bool found_raised_wall_vertex = false;
    for (const WorldVertex& v : mesh.m_vertices)
    {
        if (v.m_y > 0.0F)
        {
            found_raised_wall_vertex = true;
            break;
        }
    }
    check(found_raised_wall_vertex, "world mesh should contain raised vertices for wall geometry");

    DungeonMap symbol_map = build_test_map();
    symbol_map.m_tiles[1].m_symbol = 'D';
    symbol_map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Spawn,
        .m_col = 0,
        .m_row = 0,
        .m_debug_symbol = 'A',
        .m_collision_mask = k_tile_collision_none,
        .m_movable = true,
    });
    symbol_map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Key,
        .m_col = 2,
        .m_row = 0,
        .m_debug_symbol = 'K',
        .m_collision_mask = k_tile_collision_none,
        .m_movable = false,
    });
    symbol_map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Switch,
        .m_col = 0,
        .m_row = 1,
        .m_debug_symbol = 'S',
        .m_collision_mask = k_tile_collision_none,
        .m_movable = false,
    });

    Scene symbol_scene{};
    check(build_scene_from_dungeon_map(symbol_map, symbol_scene), "scene build should succeed for symbol mesh tests");

    const SceneNodeId key_marker_node = find_first_scene_node_by_symbol(symbol_scene, 'K');
    const SceneNodeId switch_marker_node = find_first_scene_node_by_symbol(symbol_scene, 'S');
    check(key_marker_node != k_invalid_scene_node_id, "scene should emit a marker node for key symbol K");
    check(switch_marker_node != k_invalid_scene_node_id, "scene should emit a marker node for switch symbol S");

    const uint32_t generic_runtime_visual_flags = scene_node_category_bits(SceneNodeCategory::DynamicAttachment)
        | scene_node_category_bits(SceneNodeCategory::Renderable)
        | scene_node_category_bits(SceneNodeCategory::Pickable);
    const SceneNodeId item_visual_node = add_runtime_visual_node(
        symbol_scene,
        'I',
        Float3{
            .m_x = 2.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        },
        generic_runtime_visual_flags,
        -1);
    const SceneNodeId npc_visual_node = add_runtime_visual_node(
        symbol_scene,
        'N',
        Float3{
            .m_x = 1.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        },
        generic_runtime_visual_flags,
        -1);
    check(item_visual_node != k_invalid_scene_node_id, "runtime should be able to add a generic item visual node");
    check(npc_visual_node != k_invalid_scene_node_id, "runtime should be able to add a generic npc visual node");

    const SceneNode* item_visual = find_scene_node(symbol_scene, item_visual_node);
    const SceneNode* npc_visual = find_scene_node(symbol_scene, npc_visual_node);
    check(item_visual != nullptr && item_visual->m_debug_symbol == 'I', "item runtime visual node should preserve its render symbol");
    check(npc_visual != nullptr && npc_visual->m_debug_symbol == 'N', "npc runtime visual node should preserve its render symbol");

    const SceneNodeId player_marker_node = add_runtime_marker_node(
        symbol_scene,
        'A',
        Float3{
            .m_x = 0.5F * k_scene_tile_world_size,
            .m_y = 0.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        });
    check(player_marker_node != k_invalid_scene_node_id, "runtime should be able to add a player marker scene node");
    check(
        update_scene_node_world_position(
            symbol_scene,
            player_marker_node,
            Float3{
                .m_x = 1.5F * k_scene_tile_world_size,
                .m_y = 0.5F * k_scene_tile_world_size,
                .m_z = 0.0F,
            }),
        "runtime should be able to move a player marker scene node");

    const SceneNode* moved_player_marker = find_scene_node(symbol_scene, player_marker_node);
    check(moved_player_marker != nullptr, "moved player marker scene node should remain addressable");
    check(
        moved_player_marker != nullptr
            && moved_player_marker->m_world_position.m_x == 1.5F * k_scene_tile_world_size,
        "player marker scene node should report updated world position after movement");

    const WorldMesh symbol_mesh = build_world_mesh(symbol_scene, symbol_map, 0.0F, 0.0F, 0.0F, 0.0F);

    const std::size_t expected_symbol_vertices = expected_vertices;
    const std::size_t expected_symbol_indices = expected_indices;
    check(
        symbol_mesh.m_vertices.size() == expected_symbol_vertices,
        "world mesh should exclude scene-driven marker vertices for A/K/S symbols");
    check(
        symbol_mesh.m_indices.size() == expected_symbol_indices,
        "world mesh should exclude scene-driven marker indices for A/K/S symbols");

    bool found_blue_marker = false;
    bool found_yellow_marker = false;
    bool found_white_marker = false;
    bool found_door_brown = false;
    for (const WorldVertex& v : symbol_mesh.m_vertices)
    {
        if (v.m_r == 0.20F && v.m_g == 0.45F && v.m_b == 0.95F)
        {
            found_blue_marker = true;
        }
        else if (v.m_r == 0.95F && v.m_g == 0.85F && v.m_b == 0.20F)
        {
            found_yellow_marker = true;
        }
        else if (v.m_r == 0.92F && v.m_g == 0.92F && v.m_b == 0.92F)
        {
            found_white_marker = true;
        }
        else if (v.m_r == 0.52F && v.m_g == 0.34F && v.m_b == 0.18F)
        {
            found_door_brown = true;
        }
    }

    check(!found_blue_marker, "world mesh should not bake a blue player marker into static geometry");
    check(!found_yellow_marker, "world mesh should not bake a yellow key marker into static geometry");
    check(!found_white_marker, "world mesh should not bake a white switch marker into static geometry");
    check(found_door_brown, "world mesh should render brown geometry for door symbol D");

    const WorldMesh symbol_mesh_again = build_world_mesh(symbol_scene, symbol_map, 0.0F, 0.0F, 0.0F, 0.0F);
    check(
        symbol_mesh_again.m_vertices.size() == symbol_mesh.m_vertices.size(),
        "symbol mesh build should be deterministic for vertex counts");
    check(
        symbol_mesh_again.m_indices == symbol_mesh.m_indices,
        "symbol mesh build should be deterministic for index ordering");

    DungeonMap prefab_anchor_map = build_test_map();
    prefab_anchor_map.m_prefab_placements.push_back(DungeonMap::PrefabPlacement{
        .m_prefab_id = 2U,
        .m_origin_col = 0,
        .m_origin_row = 0,
        .m_width = 2,
        .m_height = 2,
    });
    prefab_anchor_map.m_prefab_placements.push_back(DungeonMap::PrefabPlacement{
        .m_prefab_id = 3U,
        .m_origin_col = 1,
        .m_origin_row = 0,
        .m_width = 2,
        .m_height = 1,
    });

    Scene prefab_anchor_scene{};
    check(
        build_scene_from_dungeon_map(prefab_anchor_map, prefab_anchor_scene),
        "scene build should consume prefab placement metadata into runtime anchor nodes");

    std::vector<const SceneNode*> prefab_anchor_nodes{};
    for (const SceneNode& node : prefab_anchor_scene.m_nodes)
    {
        if (node.m_debug_symbol == 'F')
        {
            prefab_anchor_nodes.push_back(&node);
        }
    }

    check(
        prefab_anchor_nodes.size() == prefab_anchor_map.m_prefab_placements.size(),
        "scene build should emit one prefab anchor node for each prefab placement");
    if (prefab_anchor_nodes.size() == prefab_anchor_map.m_prefab_placements.size())
    {
        for (std::size_t i = 0; i < prefab_anchor_nodes.size(); ++i)
        {
            const SceneNode* anchor = prefab_anchor_nodes[i];
            const DungeonMap::PrefabPlacement& placement = prefab_anchor_map.m_prefab_placements[i];
            check(anchor != nullptr, "prefab anchor scene node pointer should be valid");
            if (anchor == nullptr)
            {
                continue;
            }

            check(anchor->m_payload_index == static_cast<int>(i), "prefab anchor should carry prefab placement index payload");
            check(
                scene_node_has_flags(*anchor, scene_node_category_bits(SceneNodeCategory::InteractableAnchor)),
                "prefab anchor should be tagged as interactable anchor category");
            check(
                scene_node_has_flags(*anchor, scene_node_category_bits(SceneNodeCategory::DebugOnly)),
                "prefab anchor should be tagged as debug-only category");

            const float expected_x = (static_cast<float>(placement.m_origin_col)
                + (static_cast<float>(placement.m_width) * 0.5F))
                * k_scene_tile_world_size;
            const float expected_y = (static_cast<float>(placement.m_origin_row)
                + (static_cast<float>(placement.m_height) * 0.5F))
                * k_scene_tile_world_size;

            check_near(expected_x, anchor->m_world_position.m_x, 0.0001F, "prefab anchor x should map to prefab placement center");
            check_near(expected_y, anchor->m_world_position.m_y, 0.0001F, "prefab anchor y should map to prefab placement center");
        }
    }
}

void test_wall_collision_octree_overlap_rules()
{
    DungeonMap map = build_test_map();
    map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Prop,
        .m_col = 2,
        .m_row = 0,
        .m_debug_symbol = 'T',
        .m_collision_mask = k_tile_collision_solid,
        .m_movable = false,
    });
    map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Prop,
        .m_col = 0,
        .m_row = 1,
        .m_debug_symbol = 'T',
        .m_collision_mask = k_tile_collision_solid,
        .m_movable = true,
    });

    OccupancyGrid occupancy{};
    check(build_occupancy_grid_from_map(map, occupancy), "wall collision tests should build occupancy with entity placements");
    check(
        is_tile_blocked(occupancy, 2, 0),
        "non-movable solid entities should merge into static occupancy blocking");
    check(
        !is_tile_blocked(occupancy, 0, 1),
        "movable solid entities should not merge into static occupancy blocking");

    WallCollisionOctree octree{};
    check(build_wall_collision_octree(map, octree), "wall collision octree should build for a map with walls");

    Scene scene{};
    check(build_scene_from_dungeon_map(map, scene), "scene build should succeed for wall collision tests");

    const SceneNodeId marker_node = add_runtime_marker_node(
        scene,
        'A',
        Float3{
            .m_x = 0.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        });
    check(marker_node != k_invalid_scene_node_id, "wall collision tests should create a runtime marker node");

    const SceneNode* marker = find_scene_node(scene, marker_node);
    check(marker != nullptr, "runtime marker node should be addressable for wall collision tests");
    if (marker == nullptr)
    {
        return;
    }

    const Float3 blocked_world{
        .m_x = 1.5F * k_scene_tile_world_size,
        .m_y = 0.5F * k_scene_tile_world_size,
        .m_z = 0.0F,
    };
    const Bounds3 blocked_bounds{
        .m_min = Float3{
            .m_x = blocked_world.m_x + marker->m_local_bounds.m_min.m_x,
            .m_y = blocked_world.m_y + marker->m_local_bounds.m_min.m_y,
            .m_z = blocked_world.m_z + marker->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = blocked_world.m_x + marker->m_local_bounds.m_max.m_x,
            .m_y = blocked_world.m_y + marker->m_local_bounds.m_max.m_y,
            .m_z = blocked_world.m_z + marker->m_local_bounds.m_max.m_z,
        },
    };
    check(
        wall_collision_octree_overlaps_bounds(octree, blocked_bounds),
        "wall collision octree should report overlap for a runtime scene node moved into a wall tile");

    const Float3 clear_world{
        .m_x = 2.5F * k_scene_tile_world_size,
        .m_y = 1.5F * k_scene_tile_world_size,
        .m_z = 0.0F,
    };
    const Bounds3 clear_bounds{
        .m_min = Float3{
            .m_x = clear_world.m_x + marker->m_local_bounds.m_min.m_x,
            .m_y = clear_world.m_y + marker->m_local_bounds.m_min.m_y,
            .m_z = clear_world.m_z + marker->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = clear_world.m_x + marker->m_local_bounds.m_max.m_x,
            .m_y = clear_world.m_y + marker->m_local_bounds.m_max.m_y,
            .m_z = clear_world.m_z + marker->m_local_bounds.m_max.m_z,
        },
    };
    check(
        !wall_collision_octree_overlaps_bounds(octree, clear_bounds),
        "wall collision octree should not report overlap for a runtime scene node inside clear floor space");

    const Float3 static_entity_world{
        .m_x = 2.5F * k_scene_tile_world_size,
        .m_y = 0.5F * k_scene_tile_world_size,
        .m_z = 0.0F,
    };
    const Bounds3 static_entity_bounds{
        .m_min = Float3{
            .m_x = static_entity_world.m_x + marker->m_local_bounds.m_min.m_x,
            .m_y = static_entity_world.m_y + marker->m_local_bounds.m_min.m_y,
            .m_z = static_entity_world.m_z + marker->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = static_entity_world.m_x + marker->m_local_bounds.m_max.m_x,
            .m_y = static_entity_world.m_y + marker->m_local_bounds.m_max.m_y,
            .m_z = static_entity_world.m_z + marker->m_local_bounds.m_max.m_z,
        },
    };
    check(
        wall_collision_octree_overlaps_bounds(octree, static_entity_bounds),
        "non-movable solid entities should merge into static wall collision octree surfaces");

    const Float3 movable_entity_world{
        .m_x = 0.5F * k_scene_tile_world_size,
        .m_y = 1.5F * k_scene_tile_world_size,
        .m_z = 0.0F,
    };
    const Bounds3 movable_entity_bounds{
        .m_min = Float3{
            .m_x = movable_entity_world.m_x + marker->m_local_bounds.m_min.m_x,
            .m_y = movable_entity_world.m_y + marker->m_local_bounds.m_min.m_y,
            .m_z = movable_entity_world.m_z + marker->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = movable_entity_world.m_x + marker->m_local_bounds.m_max.m_x,
            .m_y = movable_entity_world.m_y + marker->m_local_bounds.m_max.m_y,
            .m_z = movable_entity_world.m_z + marker->m_local_bounds.m_max.m_z,
        },
    };
    check(
        !wall_collision_octree_overlaps_bounds(octree, movable_entity_bounds),
        "movable solid entities should stay out of static wall collision octree surfaces");
}

void test_entity_dynamic_blocking_layer_rules()
{
    DungeonMap map = build_test_map();
    map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Prop,
        .m_col = 2,
        .m_row = 1,
        .m_debug_symbol = 'T',
        .m_collision_mask = k_tile_collision_solid,
        .m_movable = false,
    });
    map.m_entity_placements.push_back(DungeonMap::EntityPlacement{
        .m_kind = DungeonMap::EntityKind::Prop,
        .m_col = 0,
        .m_row = 1,
        .m_debug_symbol = 'M',
        .m_collision_mask = k_tile_collision_solid,
        .m_movable = true,
    });

    Scene scene{};
    check(build_scene_from_dungeon_map(map, scene), "entity dynamic blocking tests should build scene");

    const SceneNodeId static_prop_node = find_first_scene_node_by_symbol(scene, 'T');
    const SceneNodeId movable_prop_node = find_first_scene_node_by_symbol(scene, 'M');
    check(static_prop_node != k_invalid_scene_node_id, "non-movable solid entity should exist as a scene visual node");
    check(movable_prop_node != k_invalid_scene_node_id, "movable solid entity should exist as a scene visual node");

    const SceneNode* static_prop = find_scene_node(scene, static_prop_node);
    const SceneNode* movable_prop = find_scene_node(scene, movable_prop_node);
    check(static_prop != nullptr, "non-movable solid entity node should be addressable");
    check(movable_prop != nullptr, "movable solid entity node should be addressable");
    if (static_prop == nullptr || movable_prop == nullptr)
    {
        return;
    }

    const uint32_t blocking_flag = scene_node_category_bits(SceneNodeCategory::BlocksMovement);
    check(
        !scene_node_has_flags(*static_prop, blocking_flag),
        "non-movable solid entities should not be tagged as dynamic scene blockers");
    check(
        scene_node_has_flags(*movable_prop, blocking_flag),
        "movable solid entities should be tagged as dynamic scene blockers");
}

void test_scene_blocking_node_rules()
{
    const DungeonMap map = build_test_map();
    Scene scene{};
    check(build_scene_from_dungeon_map(map, scene), "scene build should succeed for scene blocking tests");

    const uint32_t non_blocking_visual_flags = scene_node_category_bits(SceneNodeCategory::DynamicAttachment)
        | scene_node_category_bits(SceneNodeCategory::Renderable)
        | scene_node_category_bits(SceneNodeCategory::Pickable);
    const uint32_t blocking_visual_flags = non_blocking_visual_flags
        | scene_node_category_bits(SceneNodeCategory::BlocksMovement);

    const SceneNodeId player_node = add_runtime_marker_node(
        scene,
        'A',
        Float3{
            .m_x = 0.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        });
    check(player_node != k_invalid_scene_node_id, "scene blocking tests should create a player marker node");

    const SceneNodeId key_node = add_runtime_visual_node(
        scene,
        'K',
        Float3{
            .m_x = 1.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        },
        non_blocking_visual_flags,
        -1);
    const SceneNodeId switch_node = add_runtime_visual_node(
        scene,
        'S',
        Float3{
            .m_x = 2.5F * k_scene_tile_world_size,
            .m_y = 1.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        },
        non_blocking_visual_flags,
        -1);
    const SceneNodeId statue_node = add_runtime_visual_node(
        scene,
        'T',
        Float3{
            .m_x = 2.5F * k_scene_tile_world_size,
            .m_y = 0.5F * k_scene_tile_world_size,
            .m_z = 0.0F,
        },
        blocking_visual_flags,
        -1);

    check(key_node != k_invalid_scene_node_id, "scene blocking tests should create a non-blocking key node");
    check(switch_node != k_invalid_scene_node_id, "scene blocking tests should create a non-blocking switch node");
    check(statue_node != k_invalid_scene_node_id, "scene blocking tests should create a blocking statue node");

    const SceneNode* key = find_scene_node(scene, key_node);
    const SceneNode* statue = find_scene_node(scene, statue_node);
    check(key != nullptr, "scene blocking tests should find key node");
    check(statue != nullptr, "scene blocking tests should find statue node");
    if (key == nullptr || statue == nullptr)
    {
        return;
    }

    const Bounds3 key_bounds{
        .m_min = Float3{
            .m_x = key->m_world_position.m_x + key->m_local_bounds.m_min.m_x,
            .m_y = key->m_world_position.m_y + key->m_local_bounds.m_min.m_y,
            .m_z = key->m_world_position.m_z + key->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = key->m_world_position.m_x + key->m_local_bounds.m_max.m_x,
            .m_y = key->m_world_position.m_y + key->m_local_bounds.m_max.m_y,
            .m_z = key->m_world_position.m_z + key->m_local_bounds.m_max.m_z,
        },
    };
    const Bounds3 statue_bounds{
        .m_min = Float3{
            .m_x = statue->m_world_position.m_x + statue->m_local_bounds.m_min.m_x,
            .m_y = statue->m_world_position.m_y + statue->m_local_bounds.m_min.m_y,
            .m_z = statue->m_world_position.m_z + statue->m_local_bounds.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = statue->m_world_position.m_x + statue->m_local_bounds.m_max.m_x,
            .m_y = statue->m_world_position.m_y + statue->m_local_bounds.m_max.m_y,
            .m_z = statue->m_world_position.m_z + statue->m_local_bounds.m_max.m_z,
        },
    };

    const uint32_t blocking_flag = scene_node_category_bits(SceneNodeCategory::BlocksMovement);
    check(
        !any_scene_node_blocks_bounds(scene, key_bounds, player_node, blocking_flag),
        "non-blocking key and switch visuals should not block movement bounds checks");
    check(
        any_scene_node_blocks_bounds(scene, statue_bounds, player_node, blocking_flag),
        "blocking statue-style visuals should block movement bounds checks");
}

void test_illusory_wall_layer_rules()
{
    DungeonMap map = build_test_map();
    // Convert the physical wall tile into a visual-only (illusory) wall.
    map.m_tiles[1].m_symbol = 'W';
    map.m_tiles[1].m_collision_mask = k_tile_collision_none;

    OccupancyGrid occupancy{};
    check(build_occupancy_grid_from_map(map, occupancy), "illusory wall test occupancy build should succeed");
    check(!is_tile_blocked(occupancy, 1, 0), "visual-only illusory wall should not block physical movement");

    WallCollisionOctree octree{};
    check(
        !build_wall_collision_octree(map, octree),
        "illusory wall map should not build physical wall octree surfaces when no physical walls exist");

    Scene scene{};
    check(build_scene_from_dungeon_map(map, scene), "illusory wall scene build should succeed");
    const WorldMesh mesh = build_world_mesh(scene, map, 0.0F, 0.0F, 0.0F, 0.0F);

    bool found_raised_visual_wall = false;
    for (const WorldVertex& v : mesh.m_vertices)
    {
        if (v.m_y > 0.0F)
        {
            found_raised_visual_wall = true;
            break;
        }
    }

    check(found_raised_visual_wall, "illusory wall should still contribute raised visual geometry");
}

void test_room_corridor_generation_rules()
{
    const DungeonGenerationConfig config{
        .m_width = 40,
        .m_height = 30,
        .m_room_count = 8,
        .m_min_room_size = 4,
        .m_max_room_size = 8,
        .m_seed = 4242U,
    };

    DungeonMap generated_a{};
    DungeonMap generated_b{};
    check(generate_room_corridor_dungeon_map(config, generated_a), "generator should succeed for valid config");
    check(generate_room_corridor_dungeon_map(config, generated_b), "generator should be repeatable for valid config");

    check(generated_a.m_width == config.m_width, "generated map should keep configured width");
    check(generated_a.m_height == config.m_height, "generated map should keep configured height");
    check(
        generated_a.m_tiles.size() == static_cast<std::size_t>(config.m_width * config.m_height),
        "generated map should fill full tile grid");

    check(generated_a.m_tiles.size() == generated_b.m_tiles.size(), "deterministic runs should have same tile count");
    check(
        generated_a.m_generated_constraints.size() == generated_b.m_generated_constraints.size(),
        "deterministic runs should have same number of generated constraints");
    check(
        generated_a.m_prefab_placements.size() == generated_b.m_prefab_placements.size(),
        "deterministic runs should have same number of prefab placements");
    check(
        generated_a.m_entity_placements.size() == generated_b.m_entity_placements.size(),
        "deterministic runs should have same number of entity placements");

    std::size_t floor_count = 0;
    std::size_t wall_count = 0;
    for (std::size_t i = 0; i < generated_a.m_tiles.size(); ++i)
    {
        const DungeonTile& ta = generated_a.m_tiles[i];
        const DungeonTile& tb = generated_b.m_tiles[i];

        check(ta.m_symbol == tb.m_symbol, "deterministic runs should have identical tile symbols");
        check(
            ta.m_collision_mask == tb.m_collision_mask,
            "deterministic runs should match tile collision masks");

        if (dungeon_tile_blocks_physical(ta))
        {
            ++wall_count;
        }
        else
        {
            ++floor_count;
        }
    }

    check(floor_count > 0, "generated map should include walkable floor tiles");
    check(wall_count > 0, "generated map should include wall tiles");

    check(
        !generated_a.m_generated_constraints.empty(),
        "generator should place at least one key/switch/door constraint");

    if (!generated_a.m_generated_constraints.empty())
    {
        const DungeonMap::DoorConstraint& c = generated_a.m_generated_constraints.front();
        const DungeonMap::DoorConstraint& c_again = generated_b.m_generated_constraints.front();

        check(c.m_key_id != 0U, "generated constraint key id should be non-zero");
        check(c.m_key_id == c_again.m_key_id, "deterministic runs should preserve key id");
        check(c.m_door_col == c_again.m_door_col && c.m_door_row == c_again.m_door_row,
            "deterministic runs should preserve door placement");
        check(c.m_key_col == c_again.m_key_col && c.m_key_row == c_again.m_key_row,
            "deterministic runs should preserve key placement");
        check(c.m_switch_col == c_again.m_switch_col && c.m_switch_row == c_again.m_switch_row,
            "deterministic runs should preserve switch placement");

        const std::size_t door_idx = static_cast<std::size_t>(c.m_door_row * generated_a.m_width + c.m_door_col);
        const std::size_t key_idx = static_cast<std::size_t>(c.m_key_row * generated_a.m_width + c.m_key_col);
        const std::size_t switch_idx = static_cast<std::size_t>(c.m_switch_row * generated_a.m_width + c.m_switch_col);

        check(generated_a.m_tiles[door_idx].m_symbol == 'D', "constraint door tile should use 'D' symbol");
        check(
            dungeon_tile_blocks_physical(generated_a.m_tiles[door_idx]),
            "constraint door tile should block movement");
        check(generated_a.m_tiles[key_idx].m_symbol == '.', "constraint key tile should remain floor mesh tile");
        check(
            !dungeon_tile_blocks_physical(generated_a.m_tiles[key_idx]),
            "constraint key tile should be walkable");
        check(generated_a.m_tiles[switch_idx].m_symbol == '.', "constraint switch tile should remain floor mesh tile");
        check(
            !dungeon_tile_blocks_physical(generated_a.m_tiles[switch_idx]),
            "constraint switch tile should be walkable");

        bool found_key_entity = false;
        bool found_switch_entity = false;
        for (const DungeonMap::EntityPlacement& entity : generated_a.m_entity_placements)
        {
            if (entity.m_kind == DungeonMap::EntityKind::Key
                && entity.m_col == c.m_key_col
                && entity.m_row == c.m_key_row)
            {
                found_key_entity = true;
            }

            if (entity.m_kind == DungeonMap::EntityKind::Switch
                && entity.m_col == c.m_switch_col
                && entity.m_row == c.m_switch_row)
            {
                found_switch_entity = true;
            }
        }

        check(found_key_entity, "constraint key placement should be represented in entity placement table");
        check(found_switch_entity, "constraint switch placement should be represented in entity placement table");
    }

    if (!generated_a.m_prefab_placements.empty())
    {
        const DungeonMap::PrefabPlacement& p = generated_a.m_prefab_placements.front();
        const DungeonMap::PrefabPlacement& p_again = generated_b.m_prefab_placements.front();

        check(p.m_prefab_id != 0U, "prefab placement id should be non-zero");
        check(p.m_prefab_id == p_again.m_prefab_id, "deterministic runs should preserve prefab id");
        check(
            p.m_origin_col == p_again.m_origin_col && p.m_origin_row == p_again.m_origin_row,
            "deterministic runs should preserve prefab origin");
        check(
            p.m_width == p_again.m_width && p.m_height == p_again.m_height,
            "deterministic runs should preserve prefab dimensions");

        check(p.m_width > 0 && p.m_height > 0, "prefab placement dimensions should be positive");
        check(p.m_origin_col >= 0 && p.m_origin_row >= 0, "prefab placement origin should be in bounds");
        check(
            p.m_origin_col + p.m_width <= generated_a.m_width
            && p.m_origin_row + p.m_height <= generated_a.m_height,
            "prefab placement bounds should fit within generated map");

        bool found_prefab_entity = false;
        for (const DungeonMap::EntityPlacement& entity : generated_a.m_entity_placements)
        {
            if (entity.m_kind != DungeonMap::EntityKind::Prop)
            {
                continue;
            }

            if (entity.m_col >= p.m_origin_col
                && entity.m_col < (p.m_origin_col + p.m_width)
                && entity.m_row >= p.m_origin_row
                && entity.m_row < (p.m_origin_row + p.m_height))
            {
                found_prefab_entity = true;
                break;
            }
        }
        check(found_prefab_entity, "prefab placement area should include prop entities");
    }

    // Baseline connectivity: all floor tiles are reachable from the first floor tile.
    int start_idx = -1;
    for (int i = 0; i < static_cast<int>(generated_a.m_tiles.size()); ++i)
    {
        if (!dungeon_tile_blocks_physical(generated_a.m_tiles[static_cast<std::size_t>(i)]))
        {
            start_idx = i;
            break;
        }
    }
    check(start_idx >= 0, "generated map should contain a floor tile start for connectivity test");

    if (start_idx >= 0)
    {
        std::vector<uint8_t> visited(generated_a.m_tiles.size(), 0U);
        std::queue<int> open{};
        visited[static_cast<std::size_t>(start_idx)] = 1U;
        open.push(start_idx);

        auto in_bounds = [&generated_a](int col, int row) {
            return col >= 0 && row >= 0 && col < generated_a.m_width && row < generated_a.m_height;
        };

        std::size_t reached_floor_count = 0;
        while (!open.empty())
        {
            const int idx = open.front();
            open.pop();

            const DungeonTile& tile = generated_a.m_tiles[static_cast<std::size_t>(idx)];
            if (!dungeon_tile_blocks_physical(tile))
            {
                ++reached_floor_count;
            }

            const int col = tile.m_col;
            const int row = tile.m_row;
            const int neighbor_cols[4] = {col + 1, col - 1, col, col};
            const int neighbor_rows[4] = {row, row, row + 1, row - 1};

            for (int n = 0; n < 4; ++n)
            {
                const int nc = neighbor_cols[n];
                const int nr = neighbor_rows[n];
                if (!in_bounds(nc, nr))
                {
                    continue;
                }

                const int nidx = nr * generated_a.m_width + nc;
                const std::size_t nidx_u = static_cast<std::size_t>(nidx);
                if (visited[nidx_u] != 0U || dungeon_tile_blocks_physical(generated_a.m_tiles[nidx_u]))
                {
                    continue;
                }

                visited[nidx_u] = 1U;
                open.push(nidx);
            }
        }

        check(
            reached_floor_count == floor_count,
            "generated floor tiles should be connected by room/corridor carve path");
    }
}

void test_generated_dungeon_validation_rules()
{
    const DungeonGenerationConfig config{
        .m_width = 40,
        .m_height = 30,
        .m_room_count = 8,
        .m_min_room_size = 4,
        .m_max_room_size = 8,
        .m_seed = 9001U,
    };

    DungeonMap generated{};
    check(generate_room_corridor_dungeon_map(config, generated), "validation test map generation should succeed");

    DungeonValidationReport valid_report{};
    check(validate_generated_dungeon_map(generated, valid_report), "generated map should pass validation");
    check(valid_report.m_is_valid, "validation report should mark generated map valid");
    check(valid_report.m_is_solvable_with_unlocks, "generated map should be solvable with unlocks");
    check(valid_report.m_constraints_valid, "generated map constraints should be valid");
    check(valid_report.m_walkable_tile_count > 0, "validation report should count walkable tiles");
    check(valid_report.m_issues.empty(), "valid generated map should have no issues");

    // Failure case: disconnected walkable islands with no doors to bridge.
    DungeonMap disconnected{};
    disconnected.m_width = 5;
    disconnected.m_height = 5;
    disconnected.m_tiles.reserve(25U);
    for (int row = 0; row < disconnected.m_height; ++row)
    {
        for (int col = 0; col < disconnected.m_width; ++col)
        {
            disconnected.m_tiles.push_back(DungeonTile{
                .m_col = col,
                .m_row = row,
                .m_collision_mask = k_tile_collision_solid,
                .m_symbol = '#',
            });
        }
    }
    disconnected.m_tiles[0].m_collision_mask = k_tile_collision_none;
    disconnected.m_tiles[0].m_symbol = '.';
    disconnected.m_tiles[24].m_collision_mask = k_tile_collision_none;
    disconnected.m_tiles[24].m_symbol = '.';

    DungeonValidationReport disconnected_report{};
    check(
        !validate_generated_dungeon_map(disconnected, disconnected_report),
        "disconnected map should fail validation");
    check(!disconnected_report.m_is_valid, "disconnected report should be invalid");
    check(
        disconnected_report.m_reachable_with_unlocks < disconnected_report.m_walkable_tile_count,
        "disconnected report should show unreached walkable tiles");
    check(!disconnected_report.m_issues.empty(), "disconnected map should produce actionable issues");

    // Failure case: malformed generated constraint metadata.
    DungeonMap bad_constraint = generated;
    if (!bad_constraint.m_generated_constraints.empty())
    {
        bad_constraint.m_generated_constraints[0].m_key_id = 0U;
    }

    DungeonValidationReport constraint_report{};
    check(
        !validate_generated_dungeon_map(bad_constraint, constraint_report),
        "map with malformed constraint should fail validation");
    check(!constraint_report.m_constraints_valid, "constraint validation should fail for malformed constraint");
    check(!constraint_report.m_issues.empty(), "constraint validation failure should report issues");
}

void test_handcrafted_entity_table_rules()
{
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    const std::filesystem::path strict_map_path =
        temp_dir / ("mordor_strict_entities_" + std::to_string(std::rand()) + ".map");
    const std::filesystem::path legacy_map_path =
        temp_dir / ("mordor_legacy_entities_" + std::to_string(std::rand()) + ".map");

    {
        std::ofstream strict_map(strict_map_path);
        strict_map << "#####\n";
        strict_map << "#...#\n";
        strict_map << "#####\n";
        strict_map << "@entity key 1 1 K 0 0\n";
        strict_map << "@entity prop 2 1 T 1 0\n";
    }

    {
        std::ofstream legacy_map(legacy_map_path);
        legacy_map << "#####\n";
        legacy_map << "#K..#\n";
        legacy_map << "#####\n";
    }

    DungeonMap strict_loaded{};
    check(
        load_handcrafted_dungeon_map(strict_map_path.string(), strict_loaded),
        "strict handcrafted map with entity table should load");
    check(
        strict_loaded.m_entity_placements.size() == 2U,
        "strict handcrafted entity table should produce expected placement count");

    if (strict_loaded.m_entity_placements.size() >= 2U)
    {
        check(
            !dungeon_entity_blocks_physical(strict_loaded.m_entity_placements[0]),
            "non-solid entity table entries should remain non-blocking");
        check(
            dungeon_entity_blocks_physical(strict_loaded.m_entity_placements[1]),
            "solid entity table entries should set physical blocking bit");
    }

    DungeonMap legacy_loaded{};
    check(
        !load_handcrafted_dungeon_map(legacy_map_path.string(), legacy_loaded),
        "legacy entity tile symbols should be rejected in strict table mode");

    std::filesystem::remove(strict_map_path);
    std::filesystem::remove(legacy_map_path);
}

} // namespace

int main()
{
    try
    {
        test_interaction_state_machines();
        test_key_and_switch_logic();
        test_generated_constraint_runtime_binding_rules();
        test_occupancy_rules();
        test_line_of_sight_rules();
        test_directional_hearing_rules();
        test_fog_of_war_rules();
        test_perception_debug_overlay_rules();
        test_party_selection_and_command_rules();
        test_ability_pipeline_rules();
        test_inventory_pipeline_rules();
        test_hud_surface_rules();
        test_world_mesh_generation_rules();
        test_wall_collision_octree_overlap_rules();
        test_entity_dynamic_blocking_layer_rules();
        test_scene_blocking_node_rules();
        test_illusory_wall_layer_rules();
        test_room_corridor_generation_rules();
        test_generated_dungeon_validation_rules();
        test_handcrafted_entity_table_rules();
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
