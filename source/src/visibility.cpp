#include "mordor/visibility.hpp"

#include <cstdlib>

namespace mordor {

LineOfSightResult trace_line_of_sight(
    const OccupancyGrid& grid,
    TileCoord origin,
    TileCoord target)
{
    LineOfSightResult result{};

    if (!in_occupancy_bounds(grid, origin.m_col, origin.m_row)
        || !in_occupancy_bounds(grid, target.m_col, target.m_row))
    {
        return result;
    }

    int current_col = origin.m_col;
    int current_row = origin.m_row;
    const int delta_col = std::abs(target.m_col - origin.m_col);
    const int delta_row = std::abs(target.m_row - origin.m_row);
    const int step_col = origin.m_col < target.m_col ? 1 : -1;
    const int step_row = origin.m_row < target.m_row ? 1 : -1;

    int error = delta_col - delta_row;

    while (true)
    {
        result.m_traversed_tiles.push_back(TileCoord{.m_col = current_col, .m_row = current_row});

        const bool is_origin = current_col == origin.m_col && current_row == origin.m_row;
        const bool is_target = current_col == target.m_col && current_row == target.m_row;

        if (!is_origin && !is_target && is_tile_blocked(grid, current_col, current_row))
        {
            result.m_hit_blocker = true;
            result.m_first_blocking_tile = TileCoord{.m_col = current_col, .m_row = current_row};
            result.m_has_line_of_sight = false;
            return result;
        }

        if (is_target)
        {
            result.m_has_line_of_sight = true;
            return result;
        }

        const int doubled_error = 2 * error;
        if (doubled_error > -delta_row)
        {
            error -= delta_row;
            current_col += step_col;
        }
        if (doubled_error < delta_col)
        {
            error += delta_col;
            current_row += step_row;
        }
    }
}

bool has_line_of_sight(
    const OccupancyGrid& grid,
    TileCoord origin,
    TileCoord target)
{
    return trace_line_of_sight(grid, origin, target).m_has_line_of_sight;
}

} // namespace mordor