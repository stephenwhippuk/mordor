#include "mordor/visibility.hpp"

#include <cstdlib>

namespace mordor {

namespace {

bool set_blocked_result(LineOfSightResult& result, TileCoord blocking_tile)
{
    result.m_hit_blocker = true;
    result.m_first_blocking_tile = blocking_tile;
    result.m_has_line_of_sight = false;
    return false;
}

} // namespace

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

    if (origin.m_col == target.m_col && origin.m_row == target.m_row)
    {
        result.m_traversed_tiles.push_back(origin);
        result.m_has_line_of_sight = true;
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

        const bool is_target = current_col == target.m_col && current_row == target.m_row;

        if (current_col != origin.m_col || current_row != origin.m_row)
        {
            if (is_tile_blocked(grid, current_col, current_row))
            {
                set_blocked_result(
                    result,
                    TileCoord{.m_col = current_col, .m_row = current_row});
                return result;
            }

            if (is_target)
            {
                result.m_has_line_of_sight = true;
                return result;
            }
        }

        const int doubled_error = 2 * error;
        const bool step_in_col = doubled_error > -delta_row;
        const bool step_in_row = doubled_error < delta_col;
        const int next_col = step_in_col ? current_col + step_col : current_col;
        const int next_row = step_in_row ? current_row + step_row : current_row;

        if (step_in_col && step_in_row)
        {
            const TileCoord horizontal_neighbor{.m_col = next_col, .m_row = current_row};
            const TileCoord vertical_neighbor{.m_col = current_col, .m_row = next_row};
            if (is_tile_blocked(grid, horizontal_neighbor.m_col, horizontal_neighbor.m_row)
                && is_tile_blocked(grid, vertical_neighbor.m_col, vertical_neighbor.m_row))
            {
                set_blocked_result(result, horizontal_neighbor);
                return result;
            }
        }

        if (step_in_col)
        {
            error -= delta_row;
            current_col += step_col;
        }
        if (step_in_row)
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