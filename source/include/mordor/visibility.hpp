#pragma once

#include "mordor/occupancy.hpp"

#include <vector>

namespace mordor {

struct TileCoord
{
    int m_col{0};
    int m_row{0};
};

struct LineOfSightResult
{
    bool m_has_line_of_sight{false};
    bool m_hit_blocker{false};
    TileCoord m_first_blocking_tile{};
    std::vector<TileCoord> m_traversed_tiles{};
};

LineOfSightResult trace_line_of_sight(
    const OccupancyGrid& grid,
    TileCoord origin,
    TileCoord target);

bool has_line_of_sight(
    const OccupancyGrid& grid,
    TileCoord origin,
    TileCoord target);

} // namespace mordor