#include "mordor/fog_of_war.hpp"

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

std::size_t fog_index(const FogOfWarState& state, int col, int row)
{
    return static_cast<std::size_t>(row * state.m_width + col);
}

bool in_fog_bounds(const FogOfWarState& state, int col, int row)
{
    return col >= 0 && row >= 0 && col < state.m_width && row < state.m_height;
}

void set_visible_explored(FogOfWarState& state, int col, int row)
{
    if (!in_fog_bounds(state, col, row))
    {
        return;
    }

    const std::size_t idx = fog_index(state, col, row);
    state.m_visible[idx] = 1U;
    state.m_explored[idx] = 1U;
}

} // namespace

bool build_fog_of_war_state(const OccupancyGrid& grid, FogOfWarState& out_state)
{
    if (grid.m_width <= 0 || grid.m_height <= 0)
    {
        return false;
    }

    const std::size_t expected_tiles = static_cast<std::size_t>(grid.m_width) * static_cast<std::size_t>(grid.m_height);
    if (grid.m_static_blocking.size() != expected_tiles || grid.m_dynamic_blocking.size() != expected_tiles)
    {
        return false;
    }

    FogOfWarState state{};
    state.m_width = grid.m_width;
    state.m_height = grid.m_height;
    state.m_visible.assign(expected_tiles, 0U);
    state.m_explored.assign(expected_tiles, 0U);
    out_state = std::move(state);
    return true;
}

void clear_fog_of_war_visible(FogOfWarState& state)
{
    std::fill(state.m_visible.begin(), state.m_visible.end(), 0U);
}

bool is_fog_visible(const FogOfWarState& state, int col, int row)
{
    if (!in_fog_bounds(state, col, row))
    {
        return false;
    }

    return state.m_visible[fog_index(state, col, row)] != 0U;
}

bool is_fog_explored(const FogOfWarState& state, int col, int row)
{
    if (!in_fog_bounds(state, col, row))
    {
        return false;
    }

    return state.m_explored[fog_index(state, col, row)] != 0U;
}

bool refresh_fog_of_war(
    FogOfWarState& state,
    const OccupancyGrid& grid,
    const std::vector<FogOfWarObserver>& observers)
{
    if (state.m_width != grid.m_width || state.m_height != grid.m_height)
    {
        return false;
    }

    clear_fog_of_war_visible(state);

    for (const FogOfWarObserver& observer : observers)
    {
        if (!in_occupancy_bounds(grid, observer.m_tile.m_col, observer.m_tile.m_row)
            || observer.m_vision_range_tiles < 0)
        {
            continue;
        }

        const int min_col = std::max(0, observer.m_tile.m_col - observer.m_vision_range_tiles);
        const int max_col = std::min(state.m_width - 1, observer.m_tile.m_col + observer.m_vision_range_tiles);
        const int min_row = std::max(0, observer.m_tile.m_row - observer.m_vision_range_tiles);
        const int max_row = std::min(state.m_height - 1, observer.m_tile.m_row + observer.m_vision_range_tiles);

        for (int row = min_row; row <= max_row; ++row)
        {
            for (int col = min_col; col <= max_col; ++col)
            {
                const int delta_col = col - observer.m_tile.m_col;
                const int delta_row = row - observer.m_tile.m_row;
                const float distance = std::sqrt(
                    static_cast<float>(delta_col * delta_col + delta_row * delta_row));
                if (distance > static_cast<float>(observer.m_vision_range_tiles))
                {
                    continue;
                }

                const TileCoord target{.m_col = col, .m_row = row};
                if (has_line_of_sight(grid, observer.m_tile, target))
                {
                    set_visible_explored(state, col, row);
                }
            }
        }
    }

    return true;
}

} // namespace mordor