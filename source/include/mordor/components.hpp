#pragma once

#include "mordor/scene.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

using EntityId = uint32_t;

constexpr EntityId k_invalid_entity_id = 0U;

enum class Allegiance : uint8_t
{
    Neutral = 0U,
    Friendly = 1U,
    Hostile = 2U,
};

enum class InteractableKind : uint8_t
{
    Door = 0U,
    Chest = 1U,
    Trap = 2U,
    Switch = 3U,
    Container = 4U,
};

enum class InteractableState : uint8_t
{
    Inactive = 0U,
    Armed = 1U,
    Closed = 2U,
    Open = 3U,
    Locked = 4U,
    Triggered = 5U,
    Disabled = 6U,
};

struct TransformComponent
{
    EntityId m_entity_id{k_invalid_entity_id};
    SceneNodeId m_scene_node_id{k_invalid_scene_node_id};
    Float3 m_local_offset{};
};

struct ColliderComponent
{
    EntityId m_entity_id{k_invalid_entity_id};
    float m_radius{0.0F};
    float m_height{0.0F};
    bool m_blocks_movement{false};
};

struct InventoryComponent
{
    EntityId m_entity_id{k_invalid_entity_id};
    uint16_t m_capacity{0U};
    uint16_t m_item_count{0U};
};

struct ActorComponent
{
    EntityId m_entity_id{k_invalid_entity_id};
    uint16_t m_archetype_id{0U};
    uint16_t m_max_action_points{0U};
    uint16_t m_current_action_points{0U};
    float m_move_speed_units_per_second{0.0F};
    Allegiance m_allegiance{Allegiance::Neutral};
};

struct InteractableComponent
{
    EntityId m_entity_id{k_invalid_entity_id};
    InteractableKind m_kind{InteractableKind::Door};
    InteractableState m_state{InteractableState::Inactive};
    uint32_t m_state_machine_id{0U};
    uint32_t m_required_key_id{0U};
    bool m_blocks_movement_when_active{false};
};

struct RuntimeComponents
{
    std::vector<EntityId> m_entities{};
    std::vector<TransformComponent> m_transforms{};
    std::vector<ColliderComponent> m_colliders{};
    std::vector<InventoryComponent> m_inventories{};
    std::vector<ActorComponent> m_actors{};
    std::vector<InteractableComponent> m_interactables{};
};

EntityId allocate_entity(RuntimeComponents& components);
void reset_components(RuntimeComponents& components);

bool validate_actor_component(const ActorComponent& component);
bool validate_interactable_component(const InteractableComponent& component);

} // namespace mordor