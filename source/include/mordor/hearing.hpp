#pragma once

#include "mordor/scene.hpp"
#include "mordor/visibility.hpp"

#include <cstdint>

namespace mordor {

enum class HearingEventKind : uint8_t
{
    Movement = 0U,
    Interaction = 1U,
    Combat = 2U,
    Ambient = 3U,
};

struct HearingEvent
{
    HearingEventKind m_kind{HearingEventKind::Ambient};
    TileCoord m_source_tile{};
    float m_loudness{1.0F};
    float m_max_range_tiles{8.0F};
};

struct HearingListener
{
    TileCoord m_listener_tile{};
    Float3 m_forward{.m_x = 1.0F, .m_y = 0.0F, .m_z = 0.0F};
    float m_rear_gain{0.35F};
    float m_detection_threshold{0.05F};
};

struct HearingResult
{
    bool m_audible{false};
    float m_perceived_loudness{0.0F};
    float m_distance_tiles{0.0F};
    float m_directional_alignment{0.0F};
    int m_occluding_blocker_count{0};
    TileCoord m_source_tile{};
};

float hearing_distance_tiles(TileCoord from, TileCoord to);
int count_hearing_occluders(const OccupancyGrid& grid, TileCoord from, TileCoord to);

HearingResult evaluate_hearing_event(
    const OccupancyGrid& grid,
    const HearingEvent& event,
    const HearingListener& listener);

} // namespace mordor