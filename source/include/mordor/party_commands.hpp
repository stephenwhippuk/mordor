#pragma once

#include "mordor/components.hpp"
#include "mordor/occupancy.hpp"
#include "mordor/visibility.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mordor {

enum class PartyCommandType : uint8_t
{
    MoveToTile = 0U,
    InteractWithEntity = 1U,
    Wait = 2U,
};

struct PartySelectionState
{
    std::vector<EntityId> m_selected_actor_ids{};
    EntityId m_primary_actor_id{k_invalid_entity_id};
};

struct PartyCommandIntent
{
    EntityId m_issuer_entity_id{k_invalid_entity_id};
    PartyCommandType m_type{PartyCommandType::Wait};
    TileCoord m_target_tile{};
    EntityId m_target_entity_id{k_invalid_entity_id};
};

enum class PartyCommandIssueStatus : uint8_t
{
    Accepted = 0U,
    RejectedNoSelection = 1U,
    RejectedIssuerNotSelected = 2U,
    RejectedInvalidIssuer = 3U,
    RejectedNonFriendlyIssuer = 4U,
    RejectedInvalidTarget = 5U,
    RejectedBlockedTarget = 6U,
    RejectedQueueFull = 7U,
    RejectedInsufficientActionPoints = 8U,
    RejectedOccupiedTarget = 9U,
    RejectedTargetEntityNotFound = 10U,
};

struct PartyCommandIssueResult
{
    PartyCommandIssueStatus m_status{PartyCommandIssueStatus::RejectedNoSelection};
    uint16_t m_estimated_action_point_cost{0U};
    std::size_t m_queue_size{0U};
};

struct PartyCommandQueue
{
    std::vector<PartyCommandIntent> m_intents{};
    std::size_t m_max_size{32U};
};

void clear_party_selection(PartySelectionState& selection);
bool is_actor_selected(const PartySelectionState& selection, EntityId actor_id);

bool select_party_actor(
    const RuntimeComponents& components,
    EntityId actor_id,
    bool additive,
    PartySelectionState& selection);

bool deselect_party_actor(PartySelectionState& selection, EntityId actor_id);

uint16_t estimate_command_action_point_cost(const PartyCommandIntent& intent);

PartyCommandIssueResult issue_party_command(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const PartySelectionState& selection,
    const PartyCommandIntent& intent,
    PartyCommandQueue& queue);

} // namespace mordor