#pragma once

#include "mordor/fog_of_war.hpp"
#include "mordor/hearing.hpp"
#include "mordor/renderer.hpp"
#include "mordor/visibility.hpp"

#include <cstddef>
#include <vector>

namespace mordor {

std::vector<TileCoord> build_tile_line(TileCoord start, TileCoord end);

std::size_t append_fog_of_war_debug_tiles(
    const FogOfWarState& fog,
    std::vector<DebugTile>& out_tiles);

std::size_t append_line_of_sight_debug_tiles(
    const LineOfSightResult& line_of_sight,
    std::vector<DebugTile>& out_tiles);

std::size_t append_hearing_debug_tiles(
    TileCoord listener_tile,
    const HearingEvent& event,
    const HearingResult& result,
    const std::vector<TileCoord>& path_tiles,
    std::vector<DebugTile>& out_tiles);

} // namespace mordor