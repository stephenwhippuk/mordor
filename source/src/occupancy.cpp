#include "mordor/occupancy.hpp"

#include "mordor/interactions.hpp"
#include "mordor/scene.hpp"

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

std::size_t tile_index(const OccupancyGrid& grid, int col, int row)
{
    return static_cast<std::size_t>(row * grid.m_width + col);
}

const TransformComponent* find_transform(
    const std::vector<TransformComponent>& transforms,
    EntityId entity_id)
{
    auto it = std::find_if(
        transforms.begin(),
        transforms.end(),
        [entity_id](const TransformComponent& transform) {
            return transform.m_entity_id == entity_id;
        });
    return it != transforms.end() ? &(*it) : nullptr;
}

bool transform_to_tile(const TransformComponent& transform, int& out_col, int& out_row)
{
    out_col = static_cast<int>(std::floor(transform.m_local_offset.m_x / k_scene_tile_world_size));
    out_row = static_cast<int>(std::floor(transform.m_local_offset.m_y / k_scene_tile_world_size));
    return true;
}

} // namespace

bool build_occupancy_grid_from_map(const DungeonMap& map, OccupancyGrid& out_grid)
{
    if (map.m_width <= 0 || map.m_height <= 0)
    {
        return false;
    }

    const std::size_t expected_tiles =
        static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height);
    if (map.m_tiles.size() != expected_tiles)
    {
        return false;
    }

    OccupancyGrid grid{};
    grid.m_width = map.m_width;
    grid.m_height = map.m_height;
    grid.m_static_blocking.assign(expected_tiles, 0U);
    grid.m_dynamic_blocking.assign(expected_tiles, 0U);
    grid.m_occupants.assign(expected_tiles, k_invalid_entity_id);

    for (const DungeonTile& tile : map.m_tiles)
    {
        if (tile.m_col < 0 || tile.m_col >= grid.m_width || tile.m_row < 0 || tile.m_row >= grid.m_height)
        {
            return false;
        }

        const std::size_t idx = tile_index(grid, tile.m_col, tile.m_row);
        grid.m_static_blocking[idx] = dungeon_tile_blocks_physical(tile) ? 1U : 0U;
    }

    for (const DungeonMap::EntityPlacement& entity : map.m_entity_placements)
    {
        if (!dungeon_entity_blocks_physical(entity) || entity.m_movable)
        {
            continue;
        }

        if (entity.m_col < 0 || entity.m_col >= grid.m_width || entity.m_row < 0 || entity.m_row >= grid.m_height)
        {
            continue;
        }

        const std::size_t idx = tile_index(grid, entity.m_col, entity.m_row);
        grid.m_static_blocking[idx] = 1U;
    }

    out_grid = std::move(grid);
    return true;
}

void clear_dynamic_occupancy(OccupancyGrid& grid)
{
    std::fill(grid.m_dynamic_blocking.begin(), grid.m_dynamic_blocking.end(), 0U);
    std::fill(grid.m_occupants.begin(), grid.m_occupants.end(), k_invalid_entity_id);
}

bool in_occupancy_bounds(const OccupancyGrid& grid, int col, int row)
{
    return col >= 0 && row >= 0 && col < grid.m_width && row < grid.m_height;
}

bool is_tile_blocked(const OccupancyGrid& grid, int col, int row)
{
    if (!in_occupancy_bounds(grid, col, row))
    {
        return true;
    }

    const std::size_t idx = tile_index(grid, col, row);
    return grid.m_static_blocking[idx] != 0U || grid.m_dynamic_blocking[idx] != 0U;
}

bool is_tile_occupied(const OccupancyGrid& grid, int col, int row)
{
    return occupying_entity(grid, col, row) != k_invalid_entity_id;
}

EntityId occupying_entity(const OccupancyGrid& grid, int col, int row)
{
    if (!in_occupancy_bounds(grid, col, row))
    {
        return k_invalid_entity_id;
    }

    return grid.m_occupants[tile_index(grid, col, row)];
}

bool set_tile_dynamic_blocked(OccupancyGrid& grid, int col, int row, bool blocked)
{
    if (!in_occupancy_bounds(grid, col, row))
    {
        return false;
    }

    grid.m_dynamic_blocking[tile_index(grid, col, row)] = blocked ? 1U : 0U;
    return true;
}

bool set_tile_occupant(OccupancyGrid& grid, int col, int row, EntityId entity_id)
{
    if (!in_occupancy_bounds(grid, col, row) || entity_id == k_invalid_entity_id)
    {
        return false;
    }

    const std::size_t idx = tile_index(grid, col, row);
    if (grid.m_occupants[idx] != k_invalid_entity_id)
    {
        return false;
    }

    grid.m_occupants[idx] = entity_id;
    return true;
}

bool clear_tile_occupant(OccupancyGrid& grid, int col, int row, EntityId expected_entity_id)
{
    if (!in_occupancy_bounds(grid, col, row))
    {
        return false;
    }

    const std::size_t idx = tile_index(grid, col, row);
    if (expected_entity_id != k_invalid_entity_id && grid.m_occupants[idx] != expected_entity_id)
    {
        return false;
    }

    grid.m_occupants[idx] = k_invalid_entity_id;
    return true;
}

bool is_tile_walkable(const OccupancyGrid& grid, int col, int row)
{
    return !is_tile_blocked(grid, col, row) && !is_tile_occupied(grid, col, row);
}

bool apply_interactable_blocking_to_grid(
    OccupancyGrid& grid,
    const std::vector<InteractableComponent>& interactables,
    const std::vector<TransformComponent>& transforms)
{
    bool changed = false;
    for (const InteractableComponent& interactable : interactables)
    {
        const TransformComponent* transform = find_transform(transforms, interactable.m_entity_id);
        if (transform == nullptr)
        {
            continue;
        }

        int col = 0;
        int row = 0;
        if (!transform_to_tile(*transform, col, row) || !in_occupancy_bounds(grid, col, row))
        {
            continue;
        }

        const bool blocks = interactable_blocks_movement(interactable);
        const std::size_t idx = tile_index(grid, col, row);

        // Keep blocking monotonic inside this pass so iteration order cannot
        // clear a blocked tile when multiple interactables share one cell.
        const uint8_t new_value = (grid.m_dynamic_blocking[idx] != 0U || blocks) ? 1U : 0U;
        if (grid.m_dynamic_blocking[idx] != new_value)
        {
            grid.m_dynamic_blocking[idx] = new_value;
            changed = true;
        }
    }

    return changed;
}

bool apply_actor_occupancy_to_grid(
    OccupancyGrid& grid,
    const std::vector<ActorComponent>& actors,
    const std::vector<TransformComponent>& transforms)
{
    bool changed = false;
    for (const ActorComponent& actor : actors)
    {
        const TransformComponent* transform = find_transform(transforms, actor.m_entity_id);
        if (transform == nullptr)
        {
            continue;
        }

        int col = 0;
        int row = 0;
        if (!transform_to_tile(*transform, col, row) || !in_occupancy_bounds(grid, col, row))
        {
            continue;
        }

        const std::size_t idx = tile_index(grid, col, row);
        if (grid.m_occupants[idx] == k_invalid_entity_id)
        {
            grid.m_occupants[idx] = actor.m_entity_id;
            changed = true;
        }
    }

    return changed;
}

} // namespace mordor