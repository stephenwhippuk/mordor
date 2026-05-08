#include "mordor/perception_debug.hpp"

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

constexpr float k_overlay_base_inset = 20.0F;
constexpr float k_path_inset = 28.0F;

DebugTile make_tile(
    TileCoord tile,
    float inset,
    float r,
    float g,
    float b)
{
    const float tile_size = k_scene_tile_world_size;
    const float width = std::max(2.0F, tile_size - inset);
    const float height = std::max(2.0F, tile_size - inset);
    const float center_x = (static_cast<float>(tile.m_col) * tile_size) + (tile_size * 0.5F);
    const float center_y = (static_cast<float>(tile.m_row) * tile_size) + (tile_size * 0.5F);

    return DebugTile{
        .m_x = center_x,
        .m_y = center_y,
        .m_width = width,
        .m_height = height,
        .m_r = r,
        .m_g = g,
        .m_b = b,
    };
}

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

std::vector<TileCoord> build_tile_line(TileCoord start, TileCoord end)
{
    std::vector<TileCoord> line{};

    int current_col = start.m_col;
    int current_row = start.m_row;
    const int delta_col = std::abs(end.m_col - start.m_col);
    const int delta_row = std::abs(end.m_row - start.m_row);
    const int step_col = start.m_col < end.m_col ? 1 : -1;
    const int step_row = start.m_row < end.m_row ? 1 : -1;

    int error = delta_col - delta_row;

    while (true)
    {
        line.push_back(TileCoord{.m_col = current_col, .m_row = current_row});
        if (current_col == end.m_col && current_row == end.m_row)
        {
            break;
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

    return line;
}

std::size_t append_fog_of_war_debug_tiles(
    const FogOfWarState& fog,
    std::vector<DebugTile>& out_tiles)
{
    if (fog.m_width <= 0 || fog.m_height <= 0)
    {
        return 0;
    }

    const std::size_t expected_tiles = static_cast<std::size_t>(fog.m_width) * static_cast<std::size_t>(fog.m_height);
    if (fog.m_visible.size() != expected_tiles || fog.m_explored.size() != expected_tiles)
    {
        return 0;
    }

    const std::size_t start_size = out_tiles.size();
    for (int row = 0; row < fog.m_height; ++row)
    {
        for (int col = 0; col < fog.m_width; ++col)
        {
            const std::size_t idx = static_cast<std::size_t>(row * fog.m_width + col);
            if (fog.m_visible[idx] != 0U)
            {
                out_tiles.push_back(make_tile(TileCoord{.m_col = col, .m_row = row}, k_overlay_base_inset, 0.94F, 0.85F, 0.26F));
            }
            else if (fog.m_explored[idx] != 0U)
            {
                out_tiles.push_back(make_tile(TileCoord{.m_col = col, .m_row = row}, k_overlay_base_inset, 0.18F, 0.22F, 0.30F));
            }
        }
    }

    return out_tiles.size() - start_size;
}

std::size_t append_line_of_sight_debug_tiles(
    const LineOfSightResult& line_of_sight,
    std::vector<DebugTile>& out_tiles)
{
    const std::size_t start_size = out_tiles.size();

    for (const TileCoord& tile : line_of_sight.m_traversed_tiles)
    {
        out_tiles.push_back(make_tile(tile, k_path_inset, 0.20F, 0.74F, 0.93F));
    }

    if (line_of_sight.m_hit_blocker)
    {
        out_tiles.push_back(make_tile(line_of_sight.m_first_blocking_tile, k_path_inset - 4.0F, 0.87F, 0.22F, 0.20F));
    }

    return out_tiles.size() - start_size;
}

std::size_t append_hearing_debug_tiles(
    TileCoord listener_tile,
    const HearingEvent& event,
    const HearingResult& result,
    const std::vector<TileCoord>& path_tiles,
    std::vector<DebugTile>& out_tiles)
{
    const std::size_t start_size = out_tiles.size();

    const float loudness = clamp01(result.m_perceived_loudness);
    const float path_base_r = result.m_audible ? 0.20F : 0.42F;
    const float path_base_g = result.m_audible ? 0.70F : 0.42F;
    const float path_base_b = result.m_audible ? 0.26F : 0.42F;

    for (const TileCoord& tile : path_tiles)
    {
        out_tiles.push_back(make_tile(
            tile,
            k_path_inset + 4.0F,
            std::clamp(path_base_r + loudness * 0.12F, 0.0F, 1.0F),
            std::clamp(path_base_g + loudness * 0.18F, 0.0F, 1.0F),
            std::clamp(path_base_b + loudness * 0.08F, 0.0F, 1.0F)));
    }

    out_tiles.push_back(make_tile(listener_tile, k_path_inset - 6.0F, 0.18F, 0.88F, 0.90F));
    out_tiles.push_back(make_tile(event.m_source_tile, k_path_inset - 6.0F, 0.95F, 0.58F, 0.22F));

    return out_tiles.size() - start_size;
}

} // namespace mordor