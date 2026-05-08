#include "mordor/ability_pipeline.hpp"
#include "mordor/components.hpp"
#include "mordor/fog_of_war.hpp"
#include "mordor/hearing.hpp"
#include "mordor/interactions.hpp"
#include "mordor/key_switch.hpp"
#include "mordor/map.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/party_commands.hpp"
#include "mordor/perception_debug.hpp"
#include "mordor/scene.hpp"
#include "mordor/visibility.hpp"

#include <cmath>
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

} // namespace

int main()
{
    try
    {
        test_interaction_state_machines();
        test_key_and_switch_logic();
        test_occupancy_rules();
        test_line_of_sight_rules();
        test_directional_hearing_rules();
        test_fog_of_war_rules();
        test_perception_debug_overlay_rules();
        test_party_selection_and_command_rules();
        test_ability_pipeline_rules();
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
