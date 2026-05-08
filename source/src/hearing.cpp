#include "mordor/hearing.hpp"

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

constexpr float k_min_normal_length = 0.0001F;
constexpr float k_occlusion_penalty_per_blocker = 0.65F;

float clamp01(float value)
{
    return std::clamp(value, 0.0F, 1.0F);
}

float vector_length_2d(const Float3& value)
{
    return std::sqrt(value.m_x * value.m_x + value.m_y * value.m_y);
}

Float3 normalize_2d(Float3 value)
{
    const float length = vector_length_2d(value);
    if (length < k_min_normal_length)
    {
        return Float3{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F};
    }

    const float inverse_length = 1.0F / length;
    value.m_x *= inverse_length;
    value.m_y *= inverse_length;
    value.m_z = 0.0F;
    return value;
}

Float3 direction_to_source(TileCoord listener, TileCoord source)
{
    return normalize_2d(Float3{
        .m_x = static_cast<float>(source.m_col - listener.m_col),
        .m_y = static_cast<float>(source.m_row - listener.m_row),
        .m_z = 0.0F,
    });
}

float directional_gain(Float3 listener_forward, Float3 toward_source, float rear_gain)
{
    const Float3 normalized_forward = normalize_2d(listener_forward);
    const float alignment = std::clamp(
        normalized_forward.m_x * toward_source.m_x + normalized_forward.m_y * toward_source.m_y,
        -1.0F,
        1.0F);
    const float front_factor = (alignment + 1.0F) * 0.5F;
    return rear_gain + (1.0F - rear_gain) * front_factor;
}

std::size_t tile_index(const OccupancyGrid& grid, int col, int row)
{
    return static_cast<std::size_t>(row * grid.m_width + col);
}

bool is_tile_blocked_for_hearing(const OccupancyGrid& grid, int col, int row)
{
    if (!in_occupancy_bounds(grid, col, row))
    {
        return true;
    }
    const std::size_t idx = tile_index(grid, col, row);
    return grid.m_static_blocking[idx] != 0U || grid.m_dynamic_blocking[idx] != 0U;
}

} // namespace

float hearing_distance_tiles(TileCoord from, TileCoord to)
{
    const float delta_col = static_cast<float>(to.m_col - from.m_col);
    const float delta_row = static_cast<float>(to.m_row - from.m_row);
    return std::sqrt(delta_col * delta_col + delta_row * delta_row);
}

int count_hearing_occluders(const OccupancyGrid& grid, TileCoord from, TileCoord to)
{
    if (!in_occupancy_bounds(grid, from.m_col, from.m_row)
        || !in_occupancy_bounds(grid, to.m_col, to.m_row)
        || (from.m_col == to.m_col && from.m_row == to.m_row))
    {
        return 0;
    }

    int current_col = from.m_col;
    int current_row = from.m_row;
    const int delta_col = std::abs(to.m_col - from.m_col);
    const int delta_row = std::abs(to.m_row - from.m_row);
    const int step_col = from.m_col < to.m_col ? 1 : -1;
    const int step_row = from.m_row < to.m_row ? 1 : -1;

    int error = delta_col - delta_row;
    int blocker_count = 0;

    while (!(current_col == to.m_col && current_row == to.m_row))
    {
        const int doubled_error = 2 * error;
        const bool step_in_col = doubled_error > -delta_row;
        const bool step_in_row = doubled_error < delta_col;
        const int next_col = step_in_col ? current_col + step_col : current_col;
        const int next_row = step_in_row ? current_row + step_row : current_row;

        if (step_in_col && step_in_row)
        {
            const bool horizontal_blocked = is_tile_blocked_for_hearing(grid, next_col, current_row);
            const bool vertical_blocked = is_tile_blocked_for_hearing(grid, current_col, next_row);
            if (horizontal_blocked && vertical_blocked)
            {
                ++blocker_count;
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

        if (!(current_col == to.m_col && current_row == to.m_row)
            && is_tile_blocked_for_hearing(grid, current_col, current_row))
        {
            ++blocker_count;
        }
    }

    return blocker_count;
}

HearingResult evaluate_hearing_event(
    const OccupancyGrid& grid,
    const HearingEvent& event,
    const HearingListener& listener)
{
    HearingResult result{};
    result.m_source_tile = event.m_source_tile;

    if (!in_occupancy_bounds(grid, listener.m_listener_tile.m_col, listener.m_listener_tile.m_row)
        || !in_occupancy_bounds(grid, event.m_source_tile.m_col, event.m_source_tile.m_row)
        || event.m_loudness <= 0.0F
        || event.m_max_range_tiles <= 0.0F)
    {
        return result;
    }

    result.m_distance_tiles = hearing_distance_tiles(listener.m_listener_tile, event.m_source_tile);
    if (result.m_distance_tiles > event.m_max_range_tiles)
    {
        return result;
    }

    const Float3 toward_source = direction_to_source(listener.m_listener_tile, event.m_source_tile);
    const Float3 normalized_forward = normalize_2d(listener.m_forward);
    result.m_directional_alignment = std::clamp(
        normalized_forward.m_x * toward_source.m_x + normalized_forward.m_y * toward_source.m_y,
        -1.0F,
        1.0F);

    const float normalized_range = 1.0F - clamp01(result.m_distance_tiles / event.m_max_range_tiles);
    const float range_loudness = event.m_loudness * normalized_range;
    const float gain = directional_gain(listener.m_forward, toward_source, clamp01(listener.m_rear_gain));

    result.m_occluding_blocker_count =
        count_hearing_occluders(grid, listener.m_listener_tile, event.m_source_tile);

    float occlusion_multiplier = 1.0F;
    for (int i = 0; i < result.m_occluding_blocker_count; ++i)
    {
        occlusion_multiplier *= k_occlusion_penalty_per_blocker;
    }

    result.m_perceived_loudness = range_loudness * gain * occlusion_multiplier;
    result.m_audible = result.m_perceived_loudness >= listener.m_detection_threshold;
    return result;
}

} // namespace mordor