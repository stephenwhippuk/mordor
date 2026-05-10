#pragma once

#include "mordor/map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mordor {

using SceneNodeId = uint32_t;

constexpr SceneNodeId k_invalid_scene_node_id = 0;
constexpr float k_scene_tile_world_size = 48.0F;

enum class SceneNodeCategory : uint32_t
{
    None = 0U,
    StaticWorld = 1U << 0U,
    StaticGeometry = 1U << 1U,
    InteractableAnchor = 1U << 2U,
    DynamicAttachment = 1U << 3U,
    DebugOnly = 1U << 4U,
    Renderable = 1U << 5U,
    Pickable = 1U << 6U,
    BlocksMovement = 1U << 7U,
};

constexpr uint32_t scene_node_category_bits(SceneNodeCategory category)
{
    return static_cast<uint32_t>(category);
}

struct Float3
{
    float m_x{0.0F};
    float m_y{0.0F};
    float m_z{0.0F};
};

struct Bounds3
{
    Float3 m_min{};
    Float3 m_max{};
};

struct SceneNode
{
    SceneNodeId m_id{k_invalid_scene_node_id};
    SceneNodeId m_parent_id{k_invalid_scene_node_id};
    std::vector<SceneNodeId> m_child_ids{};
    Float3 m_local_position{};
    Float3 m_world_position{};
    Bounds3 m_local_bounds{};
    Bounds3 m_world_bounds{};
    uint32_t m_category_flags{scene_node_category_bits(SceneNodeCategory::None)};
    int m_payload_index{-1};
    char m_debug_symbol{'\0'};
};

struct SceneOctreeCell
{
    Bounds3 m_strict_bounds{};
    Bounds3 m_query_bounds{};
    int m_depth{0};
    std::array<int, 8> m_child_indices{-1, -1, -1, -1, -1, -1, -1, -1};
    std::vector<SceneNodeId> m_node_ids{};
};

struct SceneSpatialIndex
{
    Bounds3 m_root_bounds{};
    float m_looseness{1.5F};
    int m_max_depth{4};
    std::vector<SceneOctreeCell> m_cells{};
    std::size_t m_indexed_node_count{0};
};

struct Scene
{
    SceneNodeId m_root_node_id{k_invalid_scene_node_id};
    std::vector<SceneNode> m_nodes{};
    SceneSpatialIndex m_spatial_index{};
    mutable uint32_t m_query_generation{0U};
    mutable std::vector<uint32_t> m_query_stamps{};
};

struct SceneDebugMetrics
{
    std::size_t m_cell_count{0};
    std::size_t m_leaf_cell_count{0};
    int m_max_depth{0};
    std::size_t m_indexed_node_count{0};
};

bool build_scene_from_dungeon_map(const DungeonMap& map, Scene& out_scene);
SceneNodeId add_runtime_visual_node(
    Scene& scene,
    char symbol,
    const Float3& world_position,
    uint32_t category_flags,
    int payload_index);
SceneNodeId add_runtime_marker_node(Scene& scene, char symbol, const Float3& world_position);
bool update_scene_node_world_position(Scene& scene, SceneNodeId node_id, const Float3& world_position);
const SceneNode* find_scene_node(const Scene& scene, SceneNodeId node_id);
SceneNodeId find_first_scene_node_by_symbol(const Scene& scene, char symbol);
std::vector<SceneNodeId> query_scene_bounds(const Scene& scene, const Bounds3& bounds);
bool any_scene_node_blocks_bounds(
    const Scene& scene,
    const Bounds3& bounds,
    SceneNodeId ignored_node_id,
    uint32_t blocking_flags);
std::vector<SceneNodeId> query_scene_point(
    const Scene& scene,
    const Float3& world_point,
    uint32_t required_flags);
SceneNodeId pick_scene_node_at_point(
    const Scene& scene,
    const Float3& world_point,
    uint32_t required_flags);
SceneDebugMetrics collect_scene_debug_metrics(const Scene& scene);
bool scene_node_has_flags(const SceneNode& node, uint32_t required_flags);

} // namespace mordor