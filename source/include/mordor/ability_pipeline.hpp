#pragma once

#include "mordor/components.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/visibility.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mordor {

enum class AbilityTargetKind : uint8_t
{
    None = 0U,
    Entity = 1U,
    Tile = 2U,
};

struct AbilitySpec
{
    uint32_t m_ability_id{0U};
    AbilityTargetKind m_target_kind{AbilityTargetKind::None};
    uint16_t m_action_point_cost{0U};
    uint16_t m_max_range_tiles{0U};
    bool m_allow_empty_tile_target{false};
};

struct AbilityRequest
{
    EntityId m_issuer_entity_id{k_invalid_entity_id};
    uint32_t m_ability_id{0U};
    EntityId m_target_entity_id{k_invalid_entity_id};
    TileCoord m_target_tile{};
};

enum class AbilityIssueStatus : uint8_t
{
    Accepted = 0U,
    RejectedUnknownAbility = 1U,
    RejectedInvalidIssuer = 2U,
    RejectedInsufficientActionPoints = 3U,
    RejectedInvalidTarget = 4U,
    RejectedTargetEntityNotFound = 5U,
    RejectedOutOfRange = 6U,
    RejectedBlockedTileTarget = 7U,
    RejectedQueueFull = 8U,
};

struct AbilityIssueResult
{
    AbilityIssueStatus m_status{AbilityIssueStatus::RejectedUnknownAbility};
    uint16_t m_action_point_cost{0U};
    std::size_t m_queue_size{0U};
};

struct AbilityExecutionQueue
{
    std::vector<AbilityRequest> m_requests{};
    std::size_t m_max_size{64U};
};

void clear_ability_queue(AbilityExecutionQueue& queue);

AbilityIssueResult issue_ability_request(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const std::vector<AbilitySpec>& ability_specs,
    const AbilityRequest& request,
    AbilityExecutionQueue& queue);

} // namespace mordor