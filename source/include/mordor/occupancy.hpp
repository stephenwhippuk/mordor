#pragma once

#include "mordor/components.hpp"
#include "mordor/map.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

struct OccupancyGrid
{
    int m_width{0};
    int m_height{0};
    std::vector<uint8_t> m_static_blocking{};
    std::vector<uint8_t> m_dynamic_blocking{};
    std::vector<EntityId> m_occupants{};
};

bool build_occupancy_grid_from_map(const DungeonMap& map, OccupancyGrid& out_grid);
void clear_dynamic_occupancy(OccupancyGrid& grid);

bool in_occupancy_bounds(const OccupancyGrid& grid, int col, int row);
bool is_tile_blocked(const OccupancyGrid& grid, int col, int row);
bool is_tile_occupied(const OccupancyGrid& grid, int col, int row);
EntityId occupying_entity(const OccupancyGrid& grid, int col, int row);

bool set_tile_dynamic_blocked(OccupancyGrid& grid, int col, int row, bool blocked);
bool set_tile_occupant(OccupancyGrid& grid, int col, int row, EntityId entity_id);
bool clear_tile_occupant(OccupancyGrid& grid, int col, int row, EntityId expected_entity_id);

bool is_tile_walkable(const OccupancyGrid& grid, int col, int row);

bool apply_interactable_blocking_to_grid(
    OccupancyGrid& grid,
    const std::vector<InteractableComponent>& interactables,
    const std::vector<TransformComponent>& transforms);

bool apply_actor_occupancy_to_grid(
    OccupancyGrid& grid,
    const std::vector<ActorComponent>& actors,
    const std::vector<TransformComponent>& transforms);

} // namespace mordor