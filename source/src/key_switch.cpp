#include "mordor/key_switch.hpp"

#include "mordor/map.hpp"

#include <algorithm>

namespace mordor {

namespace {

bool tile_in_bounds(const DungeonMap& map, int col, int row)
{
    return col >= 0 && row >= 0 && col < map.m_width && row < map.m_height;
}

InteractableComponent* find_interactable_by_entity(
    std::vector<InteractableComponent>& interactables,
    EntityId entity_id)
{
    auto it = std::find_if(
        interactables.begin(),
        interactables.end(),
        [entity_id](const InteractableComponent& component) {
            return component.m_entity_id == entity_id;
        });

    return it != interactables.end() ? &(*it) : nullptr;
}

} // namespace

bool actor_has_key(const KeyRingComponent& keyring, KeyId key_id)
{
    return std::find(keyring.m_key_ids.begin(), keyring.m_key_ids.end(), key_id)
        != keyring.m_key_ids.end();
}

bool grant_key_to_actor(KeyRingComponent& keyring, KeyId key_id)
{
    if (key_id == 0U || actor_has_key(keyring, key_id))
    {
        return false;
    }

    keyring.m_key_ids.push_back(key_id);
    return true;
}

bool try_unlock_interactable(
    const KeyRingComponent& keyring,
    InteractableComponent& interactable)
{
    if (interactable.m_state != InteractableState::Locked)
    {
        return false;
    }

    if (interactable.m_required_key_id == 0U)
    {
        return apply_interaction_event(interactable, InteractionEvent::Unlock);
    }

    if (!actor_has_key(keyring, interactable.m_required_key_id))
    {
        return false;
    }

    return apply_interaction_event(interactable, InteractionEvent::Unlock);
}

bool apply_switch_links_for_state(
    const InteractableComponent& switch_component,
    const std::vector<SwitchLinkModel>& switch_links,
    std::vector<InteractableComponent>& interactables)
{
    if (switch_component.m_kind != InteractableKind::Switch)
    {
        return false;
    }

    const bool active = switch_component.m_state == InteractableState::Open;
    bool changed_any = false;

    for (const SwitchLinkModel& link : switch_links)
    {
        if (link.m_switch_entity_id != switch_component.m_entity_id)
        {
            continue;
        }

        InteractableComponent* target =
            find_interactable_by_entity(interactables, link.m_target_entity_id);
        if (target == nullptr)
        {
            continue;
        }

        const InteractionEvent event = active
            ? link.m_on_activate_event
            : link.m_on_deactivate_event;
        if (apply_interaction_event(*target, event))
        {
            changed_any = true;
        }
    }

    return changed_any;
}

bool toggle_switch_and_apply_links(
    InteractableComponent& switch_component,
    const std::vector<SwitchLinkModel>& switch_links,
    std::vector<InteractableComponent>& interactables)
{
    if (!apply_interaction_event(switch_component, InteractionEvent::Use))
    {
        return false;
    }

    (void)apply_switch_links_for_state(switch_component, switch_links, interactables);
    return true;
}

bool append_generated_constraint_runtime_models(
    const DungeonMap& map,
    EntityId entity_id_base,
    std::vector<GeneratedConstraintBinding>& out_bindings,
    std::vector<InteractableComponent>& out_interactables,
    std::vector<SwitchLinkModel>& out_switch_links,
    EntityId& out_next_entity_id)
{
    if (entity_id_base == k_invalid_entity_id)
    {
        return false;
    }

    EntityId next_entity_id = entity_id_base;

    for (const DungeonMap::DoorConstraint& constraint : map.m_generated_constraints)
    {
        if (constraint.m_key_id == 0U
            || !tile_in_bounds(map, constraint.m_door_col, constraint.m_door_row)
            || !tile_in_bounds(map, constraint.m_key_col, constraint.m_key_row)
            || !tile_in_bounds(map, constraint.m_switch_col, constraint.m_switch_row))
        {
            return false;
        }

        const EntityId door_entity_id = next_entity_id++;
        const EntityId switch_entity_id = next_entity_id++;

        out_interactables.push_back(InteractableComponent{
            .m_entity_id = door_entity_id,
            .m_kind = InteractableKind::Door,
            .m_state = InteractableState::Locked,
            .m_state_machine_id = 0U,
            .m_required_key_id = constraint.m_key_id,
            .m_blocks_movement_when_active = true,
        });

        out_interactables.push_back(InteractableComponent{
            .m_entity_id = switch_entity_id,
            .m_kind = InteractableKind::Switch,
            .m_state = InteractableState::Closed,
            .m_state_machine_id = 0U,
            .m_required_key_id = 0U,
            .m_blocks_movement_when_active = false,
        });

        out_switch_links.push_back(SwitchLinkModel{
            .m_switch_entity_id = switch_entity_id,
            .m_target_entity_id = door_entity_id,
            .m_on_activate_event = InteractionEvent::Unlock,
            .m_on_deactivate_event = InteractionEvent::Close,
        });

        out_bindings.push_back(GeneratedConstraintBinding{
            .m_key_id = constraint.m_key_id,
            .m_door_col = constraint.m_door_col,
            .m_door_row = constraint.m_door_row,
            .m_key_col = constraint.m_key_col,
            .m_key_row = constraint.m_key_row,
            .m_switch_col = constraint.m_switch_col,
            .m_switch_row = constraint.m_switch_row,
            .m_door_entity_id = door_entity_id,
            .m_switch_entity_id = switch_entity_id,
        });
    }

    out_next_entity_id = next_entity_id;
    return true;
}

bool apply_generated_constraint_door_collision_overrides(DungeonMap& map)
{
    for (const DungeonMap::DoorConstraint& constraint : map.m_generated_constraints)
    {
        if (!tile_in_bounds(map, constraint.m_door_col, constraint.m_door_row))
        {
            return false;
        }

        const std::size_t tile_idx = static_cast<std::size_t>(
            constraint.m_door_row * map.m_width + constraint.m_door_col);
        if (tile_idx >= map.m_tiles.size())
        {
            return false;
        }

        DungeonTile& tile = map.m_tiles[tile_idx];
        tile.m_collision_mask &= ~k_tile_collision_solid;
    }

    return true;
}

} // namespace mordor