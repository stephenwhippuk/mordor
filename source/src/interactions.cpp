#include "mordor/interactions.hpp"

namespace mordor {

namespace {

bool can_apply_door_event(const InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
        return component.m_state == InteractableState::Closed
            || component.m_state == InteractableState::Open;
    case InteractionEvent::Open:
        return component.m_state == InteractableState::Closed;
    case InteractionEvent::Close:
        return component.m_state == InteractableState::Open;
    case InteractionEvent::Lock:
        return component.m_state == InteractableState::Closed;
    case InteractionEvent::Unlock:
        return component.m_state == InteractableState::Locked;
    case InteractionEvent::Reset:
        return component.m_state == InteractableState::Locked;
    default:
        return false;
    }
}

bool can_apply_chest_event(const InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
    case InteractionEvent::Open:
        return component.m_state == InteractableState::Closed;
    case InteractionEvent::Close:
        return component.m_state == InteractableState::Open;
    case InteractionEvent::Lock:
        return component.m_state == InteractableState::Closed;
    case InteractionEvent::Unlock:
        return component.m_state == InteractableState::Locked;
    case InteractionEvent::Reset:
        return component.m_state == InteractableState::Locked;
    default:
        return false;
    }
}

bool can_apply_trap_event(const InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
    case InteractionEvent::Trigger:
        return component.m_state == InteractableState::Armed;
    case InteractionEvent::Disarm:
        return component.m_state == InteractableState::Armed;
    case InteractionEvent::Arm:
        return component.m_state == InteractableState::Inactive
            || component.m_state == InteractableState::Disabled;
    case InteractionEvent::Reset:
        return component.m_state == InteractableState::Triggered;
    default:
        return false;
    }
}

void apply_door_event(InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
        component.m_state = component.m_state == InteractableState::Closed
            ? InteractableState::Open
            : InteractableState::Closed;
        return;
    case InteractionEvent::Open:
        component.m_state = InteractableState::Open;
        return;
    case InteractionEvent::Close:
        component.m_state = InteractableState::Closed;
        return;
    case InteractionEvent::Lock:
        component.m_state = InteractableState::Locked;
        return;
    case InteractionEvent::Unlock:
    case InteractionEvent::Reset:
        component.m_state = InteractableState::Closed;
        return;
    default:
        return;
    }
}

void apply_chest_event(InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
    case InteractionEvent::Open:
        component.m_state = InteractableState::Open;
        return;
    case InteractionEvent::Close:
        component.m_state = InteractableState::Closed;
        return;
    case InteractionEvent::Lock:
        component.m_state = InteractableState::Locked;
        return;
    case InteractionEvent::Unlock:
    case InteractionEvent::Reset:
        component.m_state = InteractableState::Closed;
        return;
    default:
        return;
    }
}

void apply_trap_event(InteractableComponent& component, InteractionEvent event)
{
    switch (event)
    {
    case InteractionEvent::Use:
    case InteractionEvent::Trigger:
        component.m_state = InteractableState::Triggered;
        return;
    case InteractionEvent::Disarm:
        component.m_state = InteractableState::Disabled;
        return;
    case InteractionEvent::Arm:
        component.m_state = InteractableState::Armed;
        return;
    case InteractionEvent::Reset:
        component.m_state = InteractableState::Armed;
        return;
    default:
        return;
    }
}

} // namespace

bool can_apply_interaction_event(const InteractableComponent& component, InteractionEvent event)
{
    switch (component.m_kind)
    {
    case InteractableKind::Door:
        return can_apply_door_event(component, event);
    case InteractableKind::Chest:
    case InteractableKind::Container:
        return can_apply_chest_event(component, event);
    case InteractableKind::Trap:
        return can_apply_trap_event(component, event);
    case InteractableKind::Switch:
        return event == InteractionEvent::Use;
    default:
        return false;
    }
}

bool apply_interaction_event(InteractableComponent& component, InteractionEvent event)
{
    if (!can_apply_interaction_event(component, event))
    {
        return false;
    }

    switch (component.m_kind)
    {
    case InteractableKind::Door:
        apply_door_event(component, event);
        return true;
    case InteractableKind::Chest:
    case InteractableKind::Container:
        apply_chest_event(component, event);
        return true;
    case InteractableKind::Trap:
        apply_trap_event(component, event);
        return true;
    case InteractableKind::Switch:
        component.m_state = component.m_state == InteractableState::Open
            ? InteractableState::Closed
            : InteractableState::Open;
        return true;
    default:
        return false;
    }
}

bool interactable_blocks_movement(const InteractableComponent& component)
{
    if (!component.m_blocks_movement_when_active)
    {
        return false;
    }

    switch (component.m_kind)
    {
    case InteractableKind::Door:
        return component.m_state == InteractableState::Closed
            || component.m_state == InteractableState::Locked;
    case InteractableKind::Chest:
    case InteractableKind::Container:
        return component.m_state == InteractableState::Closed
            || component.m_state == InteractableState::Locked;
    case InteractableKind::Trap:
        return false;
    case InteractableKind::Switch:
        return false;
    default:
        return false;
    }
}

} // namespace mordor