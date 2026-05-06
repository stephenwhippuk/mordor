#include "mordor/components.hpp"

#include <algorithm>

namespace mordor {

EntityId allocate_entity(RuntimeComponents& components)
{
    const EntityId next_id = static_cast<EntityId>(components.m_entities.size() + 1U);
    components.m_entities.push_back(next_id);
    return next_id;
}

void reset_components(RuntimeComponents& components)
{
    components.m_entities.clear();
    components.m_transforms.clear();
    components.m_colliders.clear();
    components.m_inventories.clear();
    components.m_actors.clear();
    components.m_interactables.clear();
}

bool validate_actor_component(const ActorComponent& component)
{
    if (component.m_entity_id == k_invalid_entity_id)
    {
        return false;
    }

    if (component.m_move_speed_units_per_second <= 0.0F)
    {
        return false;
    }

    if (component.m_max_action_points == 0U)
    {
        return false;
    }

    if (component.m_current_action_points > component.m_max_action_points)
    {
        return false;
    }

    return true;
}

bool validate_interactable_component(const InteractableComponent& component)
{
    if (component.m_entity_id == k_invalid_entity_id)
    {
        return false;
    }

    if (component.m_kind == InteractableKind::Door)
    {
        const bool valid_door_state = component.m_state == InteractableState::Closed
            || component.m_state == InteractableState::Open
            || component.m_state == InteractableState::Locked;
        if (!valid_door_state)
        {
            return false;
        }
    }

    if (component.m_kind == InteractableKind::Trap)
    {
        const bool valid_trap_state = component.m_state == InteractableState::Armed
            || component.m_state == InteractableState::Triggered
            || component.m_state == InteractableState::Disabled;
        if (!valid_trap_state)
        {
            return false;
        }
    }

    return true;
}

} // namespace mordor