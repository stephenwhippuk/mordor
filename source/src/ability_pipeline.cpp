#include "mordor/ability_pipeline.hpp"

#include <algorithm>
#include <cmath>


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

const TransformComponent* find_transform(const RuntimeComponents& components, EntityId entity_id)
{
    auto it = std::find_if(
        components.m_transforms.begin(),
        components.m_transforms.end(),
        [entity_id](const TransformComponent& transform) {
            return transform.m_entity_id == entity_id;
        });
    return it != components.m_transforms.end() ? &(*it) : nullptr;
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

const AbilitySpec* find_ability_spec(const std::vector<AbilitySpec>& specs, uint32_t ability_id)
{
    auto it = std::find_if(
        specs.begin(),
        specs.end(),
        [ability_id](const AbilitySpec& spec) {
            return spec.m_ability_id == ability_id;
        });
    return it != specs.end() ? &(*it) : nullptr;
}

TileCoord transform_to_tile(const TransformComponent& transform)
{
    return TileCoord{
        .m_col = static_cast<int>(std::floor(transform.m_local_offset.m_x / k_scene_tile_world_size)),
        .m_row = static_cast<int>(std::floor(transform.m_local_offset.m_y / k_scene_tile_world_size)),
    };
}

float tile_distance(TileCoord from, TileCoord to)
{
    const int delta_col = std::abs(to.m_col - from.m_col);
    const int delta_row = std::abs(to.m_row - from.m_row);
    return static_cast<float>(std::max(delta_col, delta_row));
}

bool is_default_target_tile(const TileCoord& tile)
{
    return tile.m_col == 0 && tile.m_row == 0;
}

} // namespace

void clear_ability_queue(AbilityExecutionQueue& queue)
{
    queue.m_requests.clear();
}

AbilityIssueResult issue_ability_request(
    const RuntimeComponents& components,
    const OccupancyGrid& grid,
    const std::vector<AbilitySpec>& ability_specs,
    const AbilityRequest& request,
    AbilityExecutionQueue& queue)
{
    AbilityIssueResult result{};
    result.m_queue_size = queue.m_requests.size();

    const AbilitySpec* spec = find_ability_spec(ability_specs, request.m_ability_id);
    if (spec == nullptr)
    {
        result.m_status = AbilityIssueStatus::RejectedUnknownAbility;
        return result;
    }

    result.m_action_point_cost = spec->m_action_point_cost;

    const ActorComponent* issuer = find_actor(components, request.m_issuer_entity_id);
    if (issuer == nullptr)
    {
        result.m_status = AbilityIssueStatus::RejectedInvalidIssuer;
        return result;
    }

    if (issuer->m_current_action_points < spec->m_action_point_cost)
    {
        result.m_status = AbilityIssueStatus::RejectedInsufficientActionPoints;
        return result;
    }

    if (queue.m_requests.size() >= queue.m_max_size)
    {
        result.m_status = AbilityIssueStatus::RejectedQueueFull;
        return result;
    }

    const TransformComponent* issuer_transform = find_transform(components, request.m_issuer_entity_id);
    if (issuer_transform == nullptr)
    {
        result.m_status = AbilityIssueStatus::RejectedInvalidIssuer;
        return result;
    }

    const TileCoord issuer_tile = transform_to_tile(*issuer_transform);

    if (spec->m_target_kind == AbilityTargetKind::None)
    {
        if (request.m_target_entity_id != k_invalid_entity_id || !is_default_target_tile(request.m_target_tile))
        {
            result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
            return result;
        }
    }
    else if (spec->m_target_kind == AbilityTargetKind::Entity)
    {
        if (!is_default_target_tile(request.m_target_tile))
        {
            result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (!entity_exists(components, request.m_target_entity_id))
        {
            result.m_status = AbilityIssueStatus::RejectedTargetEntityNotFound;
            return result;
        }

        const TransformComponent* target_transform = find_transform(components, request.m_target_entity_id);
        if (target_transform == nullptr)
        {
            result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
            return result;
        }

        const TileCoord target_tile = transform_to_tile(*target_transform);
        if (tile_distance(issuer_tile, target_tile) > spec->m_max_range_tiles)
        {
            result.m_status = AbilityIssueStatus::RejectedOutOfRange;
            return result;
        }
    }
    else if (spec->m_target_kind == AbilityTargetKind::Tile)
    {
        if (request.m_target_entity_id != k_invalid_entity_id)
        {
            result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (!in_occupancy_bounds(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
        {
            result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
            return result;
        }

        if (tile_distance(issuer_tile, request.m_target_tile) > spec->m_max_range_tiles)
        {
            result.m_status = AbilityIssueStatus::RejectedOutOfRange;
            return result;
        }

        if (!spec->m_allow_empty_tile_target)
        {
            if (is_tile_blocked(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
            {
                result.m_status = AbilityIssueStatus::RejectedBlockedTileTarget;
                return result;
            }

            if (!is_tile_occupied(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
            {
                result.m_status = AbilityIssueStatus::RejectedInvalidTarget;
                return result;
            }
        }
        else
        {
            if (is_tile_blocked(grid, request.m_target_tile.m_col, request.m_target_tile.m_row))
            {
                result.m_status = AbilityIssueStatus::RejectedBlockedTileTarget;
                return result;
            }
        }
    }

    queue.m_requests.push_back(request);
    result.m_status = AbilityIssueStatus::Accepted;
    result.m_queue_size = queue.m_requests.size();
    return result;
}

} // namespace mordor