#include "mordor/key_switch.hpp"

#include <algorithm>

namespace mordor {

namespace {

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

} // namespace mordor