#pragma once

#include "mordor/interactions.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

using KeyId = uint32_t;

struct KeyRingComponent
{
    EntityId m_actor_entity_id{k_invalid_entity_id};
    std::vector<KeyId> m_key_ids{};
};

struct SwitchLinkModel
{
    EntityId m_switch_entity_id{k_invalid_entity_id};
    EntityId m_target_entity_id{k_invalid_entity_id};
    InteractionEvent m_on_activate_event{InteractionEvent::Open};
    InteractionEvent m_on_deactivate_event{InteractionEvent::Close};
};

bool actor_has_key(const KeyRingComponent& keyring, KeyId key_id);
bool grant_key_to_actor(KeyRingComponent& keyring, KeyId key_id);
bool try_unlock_interactable(
    const KeyRingComponent& keyring,
    InteractableComponent& interactable);

bool apply_switch_links_for_state(
    const InteractableComponent& switch_component,
    const std::vector<SwitchLinkModel>& switch_links,
    std::vector<InteractableComponent>& interactables);

bool toggle_switch_and_apply_links(
    InteractableComponent& switch_component,
    const std::vector<SwitchLinkModel>& switch_links,
    std::vector<InteractableComponent>& interactables);

} // namespace mordor