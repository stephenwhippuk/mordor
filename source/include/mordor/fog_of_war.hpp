#pragma once

#include "mordor/occupancy.hpp"
#include "mordor/visibility.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

struct FogOfWarObserver
{
    TileCoord m_tile{};
    int m_vision_range_tiles{0};
};

struct FogOfWarState
{
    int m_width{0};
    int m_height{0};
    std::vector<uint8_t> m_visible{};
    std::vector<uint8_t> m_explored{};
};

bool build_fog_of_war_state(const OccupancyGrid& grid, FogOfWarState& out_state);
void clear_fog_of_war_visible(FogOfWarState& state);
bool is_fog_visible(const FogOfWarState& state, int col, int row);
bool is_fog_explored(const FogOfWarState& state, int col, int row);

bool refresh_fog_of_war(
    FogOfWarState& state,
    const OccupancyGrid& grid,
    const std::vector<FogOfWarObserver>& observers);

} // namespace mordor