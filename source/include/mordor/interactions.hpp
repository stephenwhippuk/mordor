#pragma once

#include "mordor/components.hpp"

#include <cstdint>

namespace mordor {

enum class InteractionEvent : uint8_t
{
    Use = 0U,
    Open = 1U,
    Close = 2U,
    Lock = 3U,
    Unlock = 4U,
    Arm = 5U,
    Disarm = 6U,
    Trigger = 7U,
    Reset = 8U,
};

bool can_apply_interaction_event(const InteractableComponent& component, InteractionEvent event);
bool apply_interaction_event(InteractableComponent& component, InteractionEvent event);
bool interactable_blocks_movement(const InteractableComponent& component);

} // namespace mordor