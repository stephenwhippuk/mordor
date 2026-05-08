#include "mordor/party_commands.hpp"

#include <algorithm>

namespace mordor {

namespace {

const ActorComponent* find_actor(const RuntimeComponents& components, EntityId entity_id)
{
    auto it = std::find_if(
        components.m_actors.begin(),
        components.m_actors.end(),
        [entity_id](const ActorComponent& actor) {
            return actor.m_entity_id == entity_id;
        });
    return it != components.m_actors.end() ? &(*it) : nullptr;
}

bool is_friendly_actor(const ActorComponent& actor)
{
    return actor.m_allegiance == Allegiance::Friendly;
}

bool entity_exists(const RuntimeComponents& components, EntityId entity_id)
{
    if (entity_id == k_invalid_entity_id)
    {
        return false;
    }

    return std::find(components.m_entities.begin(), components.m_entities.end(), entity_id)
        != components.m_entities.end();
}

} // namespace

void clear_party_selection(PartySelectionState& selection)
{
    selection.m_selected_actor_ids.clear();
    selection.m_primary_actor_id = k_invalid_entity_id;
}

bool is_actor_selected(const PartySelectionState& selection, EntityId actor_id)
{
    return std::find(selection.m_selected_actor_ids.begin(), selection.m_selected_actor_ids.end(), actor_id)
        != selection.m_selected_actor_ids.end();
}

bool select_party_actor(
    const RuntimeComponents& components,
    EntityId actor_id,
    bool additive,
    PartySelectionState& selection)
{
    const ActorComponent* actor = find_actor(components, actor_id);
    if (actor == nullptr || !is_friendly_actor(*actor))
    {
        return false;
    }

    if (!additive)
    {
        clear_party_selection(selection);
    }

    if (!is_actor_selected(selection, actor_id))
    {
        selection.m_selected_actor_ids.push_back(actor_id);
    }

    selection.m_primary_actor_id = actor_id;
    return true;
}

bool deselect_party_actor(PartySelectionState& selection, EntityId actor_id)
{
    auto it = std::find(selection.m_selected_actor_ids.begin(), selection.m_selected_actor_ids.end(), actor_id);
    if (it == selection.m_selected_actor_ids.end())
    {
        return false;
    }

    selection.m_selected_actor_ids.erase(it);

    if (selection.m_primary_actor_id == actor_id)
    {
        selection.m_primary_actor_id = selection.m_selected_actor_ids.empty()
            ? k_invalid_entity_id
            : selection.m_selected_actor_ids.front();
    }

    return true;
}

uint16_t estimate_command_action_point_cost(const PartyCommandIntent& intent)
{
    switch (intent.m_type)
    {
    case PartyCommandType::MoveToTile:
        return 1U;
    case PartyCommandType::InteractWithEntity:
        return 1U;
    case PartyCommandType::Wait:
        return 0U;
    default:
        return 0U;
    }
}

PartyCommandIssueResult issue_party_command(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const PartySelectionState& selection,
    const PartyCommandIntent& intent,
    PartyCommandQueue& queue)
{
    PartyCommandIssueResult result{};
    result.m_estimated_action_point_cost = estimate_command_action_point_cost(intent);
    result.m_queue_size = queue.m_intents.size();

    if (selection.m_selected_actor_ids.empty())
    {
        result.m_status = PartyCommandIssueStatus::RejectedNoSelection;
        return result;
    }

    if (!is_actor_selected(selection, intent.m_issuer_entity_id))
    {
        result.m_status = PartyCommandIssueStatus::RejectedIssuerNotSelected;
        return result;
    }

    const ActorComponent* actor = find_actor(components, intent.m_issuer_entity_id);
    if (actor == nullptr)
    {
        result.m_status = PartyCommandIssueStatus::RejectedInvalidIssuer;
        return result;
    }

    if (!is_friendly_actor(*actor))
    {
        result.m_status = PartyCommandIssueStatus::RejectedNonFriendlyIssuer;
        return result;
    }

    if (actor->m_current_action_points < result.m_estimated_action_point_cost)
    {
        result.m_status = PartyCommandIssueStatus::RejectedInsufficientActionPoints;
        return result;
    }

    if (queue.m_intents.size() >= queue.m_max_size)
    {
        result.m_status = PartyCommandIssueStatus::RejectedQueueFull;
        return result;
    }

    if (intent.m_type == PartyCommandType::MoveToTile)
    {
        if (!in_occupancy_bounds(grid, intent.m_target_tile.m_col, intent.m_target_tile.m_row))
        {
            result.m_status = PartyCommandIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (is_tile_blocked(grid, intent.m_target_tile.m_col, intent.m_target_tile.m_row))
        {
            result.m_status = PartyCommandIssueStatus::RejectedBlockedTarget;
            return result;
        }

        if (is_tile_occupied(grid, intent.m_target_tile.m_col, intent.m_target_tile.m_row))
        {
            result.m_status = PartyCommandIssueStatus::RejectedOccupiedTarget;
            return result;
        }
    }
    else if (intent.m_type == PartyCommandType::InteractWithEntity)
    {
        if (!entity_exists(components, intent.m_target_entity_id))
        {
            result.m_status = PartyCommandIssueStatus::RejectedTargetEntityNotFound;
            return result;
        }
    }

    queue.m_intents.push_back(intent);
    result.m_status = PartyCommandIssueStatus::Accepted;
    result.m_queue_size = queue.m_intents.size();
    return result;
}

} // namespace mordor