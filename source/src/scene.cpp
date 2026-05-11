#include "mordor/scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mordor {

namespace {

constexpr int k_scene_chunk_tile_size = 4;
constexpr float k_default_scene_depth_min = -1.0F;
constexpr float k_default_scene_depth_max = 1.0F;

struct SceneNodeCreateInfo
{
    SceneNodeId m_parent_id{k_invalid_scene_node_id};
    Float3 m_local_position{};
    Bounds3 m_local_bounds{};
    uint32_t m_category_flags{scene_node_category_bits(SceneNodeCategory::None)};
    int m_payload_index{-1};
    char m_debug_symbol{'\0'};
};

constexpr float k_runtime_marker_half_extent = 10.0F;

Float3 add_float3(const Float3& lhs, const Float3& rhs)
{
    return Float3{
        .m_x = lhs.m_x + rhs.m_x,
        .m_y = lhs.m_y + rhs.m_y,
        .m_z = lhs.m_z + rhs.m_z,
    };
}

Bounds3 translate_bounds(const Bounds3& bounds, const Float3& offset)
{
    return Bounds3{
        .m_min = add_float3(bounds.m_min, offset),
        .m_max = add_float3(bounds.m_max, offset),
    };
}

bool bounds_intersect(const Bounds3& lhs, const Bounds3& rhs)
{
    return lhs.m_min.m_x <= rhs.m_max.m_x && lhs.m_max.m_x >= rhs.m_min.m_x
        && lhs.m_min.m_y <= rhs.m_max.m_y && lhs.m_max.m_y >= rhs.m_min.m_y
        && lhs.m_min.m_z <= rhs.m_max.m_z && lhs.m_max.m_z >= rhs.m_min.m_z;
}

bool contains_bounds(const Bounds3& outer, const Bounds3& inner)
{
    return outer.m_min.m_x <= inner.m_min.m_x && outer.m_max.m_x >= inner.m_max.m_x
        && outer.m_min.m_y <= inner.m_min.m_y && outer.m_max.m_y >= inner.m_max.m_y
        && outer.m_min.m_z <= inner.m_min.m_z && outer.m_max.m_z >= inner.m_max.m_z;
}

bool point_in_bounds(const Float3& point, const Bounds3& bounds)
{
    return point.m_x >= bounds.m_min.m_x && point.m_x <= bounds.m_max.m_x
        && point.m_y >= bounds.m_min.m_y && point.m_y <= bounds.m_max.m_y
        && point.m_z >= bounds.m_min.m_z && point.m_z <= bounds.m_max.m_z;
}

Bounds3 expand_bounds(const Bounds3& bounds, float factor)
{
    const Float3 center{
        .m_x = (bounds.m_min.m_x + bounds.m_max.m_x) * 0.5F,
        .m_y = (bounds.m_min.m_y + bounds.m_max.m_y) * 0.5F,
        .m_z = (bounds.m_min.m_z + bounds.m_max.m_z) * 0.5F,
    };
    const Float3 half_extent{
        .m_x = ((bounds.m_max.m_x - bounds.m_min.m_x) * 0.5F) * factor,
        .m_y = ((bounds.m_max.m_y - bounds.m_min.m_y) * 0.5F) * factor,
        .m_z = ((bounds.m_max.m_z - bounds.m_min.m_z) * 0.5F) * factor,
    };

    return Bounds3{
        .m_min = Float3{
            .m_x = center.m_x - half_extent.m_x,
            .m_y = center.m_y - half_extent.m_y,
            .m_z = center.m_z - half_extent.m_z,
        },
        .m_max = Float3{
            .m_x = center.m_x + half_extent.m_x,
            .m_y = center.m_y + half_extent.m_y,
            .m_z = center.m_z + half_extent.m_z,
        },
    };
}

Bounds3 child_strict_bounds(const Bounds3& parent, int octant)
{
    const Float3 center{
        .m_x = (parent.m_min.m_x + parent.m_max.m_x) * 0.5F,
        .m_y = (parent.m_min.m_y + parent.m_max.m_y) * 0.5F,
        .m_z = (parent.m_min.m_z + parent.m_max.m_z) * 0.5F,
    };

    const bool upper_x = (octant & 1) != 0;
    const bool upper_y = (octant & 2) != 0;
    const bool upper_z = (octant & 4) != 0;

    return Bounds3{
        .m_min = Float3{
            .m_x = upper_x ? center.m_x : parent.m_min.m_x,
            .m_y = upper_y ? center.m_y : parent.m_min.m_y,
            .m_z = upper_z ? center.m_z : parent.m_min.m_z,
        },
        .m_max = Float3{
            .m_x = upper_x ? parent.m_max.m_x : center.m_x,
            .m_y = upper_y ? parent.m_max.m_y : center.m_y,
            .m_z = upper_z ? parent.m_max.m_z : center.m_z,
        },
    };
}

SceneNodeId add_scene_node(Scene& scene, const SceneNodeCreateInfo& create_info)
{
    Float3 world_position = create_info.m_local_position;
    if (create_info.m_parent_id != k_invalid_scene_node_id)
    {
        const SceneNode* parent = find_scene_node(scene, create_info.m_parent_id);
        if (parent == nullptr)
        {
            return k_invalid_scene_node_id;
        }
        world_position = add_float3(parent->m_world_position, create_info.m_local_position);
    }

    const SceneNodeId node_id = static_cast<SceneNodeId>(scene.m_nodes.size() + 1U);
    scene.m_nodes.push_back(SceneNode{
        .m_id = node_id,
        .m_parent_id = create_info.m_parent_id,
        .m_child_ids = {},
        .m_local_position = create_info.m_local_position,
        .m_world_position = world_position,
        .m_local_bounds = create_info.m_local_bounds,
        .m_world_bounds = translate_bounds(create_info.m_local_bounds, world_position),
        .m_category_flags = create_info.m_category_flags,
        .m_payload_index = create_info.m_payload_index,
        .m_debug_symbol = create_info.m_debug_symbol,
    });

    if (create_info.m_parent_id != k_invalid_scene_node_id)
    {
        SceneNode& parent = scene.m_nodes[static_cast<std::size_t>(create_info.m_parent_id - 1U)];
        parent.m_child_ids.push_back(node_id);
    }

    return node_id;
}

int ensure_child_cell(SceneSpatialIndex& index, int parent_cell_index, int octant)
{
    const std::size_t parent_index = static_cast<std::size_t>(parent_cell_index);
    const int existing = index.m_cells[parent_index].m_child_indices[static_cast<std::size_t>(octant)];
    if (existing >= 0)
    {
        return existing;
    }

    const Bounds3 strict_bounds = child_strict_bounds(index.m_cells[parent_index].m_strict_bounds, octant);
    const int child_index = static_cast<int>(index.m_cells.size());
    index.m_cells.push_back(SceneOctreeCell{
        .m_strict_bounds = strict_bounds,
        .m_query_bounds = expand_bounds(strict_bounds, index.m_looseness),
        .m_depth = index.m_cells[parent_index].m_depth + 1,
        .m_child_indices = {-1, -1, -1, -1, -1, -1, -1, -1},
        .m_node_ids = {},
    });
    index.m_cells[parent_index].m_child_indices[static_cast<std::size_t>(octant)] = child_index;
    return child_index;
}

void insert_scene_node(Scene& scene, int cell_index, SceneNodeId node_id)
{
    SceneSpatialIndex& index = scene.m_spatial_index;
    if (cell_index < 0 || static_cast<std::size_t>(cell_index) >= index.m_cells.size())
    {
        return;
    }

    SceneOctreeCell& cell = index.m_cells[static_cast<std::size_t>(cell_index)];
    const SceneNode* node = find_scene_node(scene, node_id);
    if (node == nullptr)
    {
        return;
    }

    if (cell.m_depth < index.m_max_depth)
    {
        int child_octant = -1;
        for (int octant = 0; octant < 8; ++octant)
        {
            const Bounds3 candidate_bounds =
                expand_bounds(child_strict_bounds(cell.m_strict_bounds, octant), index.m_looseness);
            if (!contains_bounds(candidate_bounds, node->m_world_bounds))
            {
                continue;
            }

            if (child_octant >= 0)
            {
                child_octant = -1;
                break;
            }
            child_octant = octant;
        }

        if (child_octant >= 0)
        {
            const int child_cell_index = ensure_child_cell(index, cell_index, child_octant);
            insert_scene_node(scene, child_cell_index, node_id);
            return;
        }
    }

    cell.m_node_ids.push_back(node_id);
    ++index.m_indexed_node_count;
}

void rebuild_scene_spatial_index(Scene& scene, const Bounds3& root_bounds)
{
    const float spatial_index_looseness = 1.5F;
    scene.m_spatial_index = SceneSpatialIndex{
        .m_root_bounds = root_bounds,
        .m_looseness = spatial_index_looseness,
        .m_max_depth = 4,
        .m_cells = {SceneOctreeCell{
            .m_strict_bounds = root_bounds,
            .m_query_bounds = expand_bounds(root_bounds, spatial_index_looseness),
            .m_depth = 0,
            .m_child_indices = {-1, -1, -1, -1, -1, -1, -1, -1},
            .m_node_ids = {},
        }},
        .m_indexed_node_count = 0,
    };

    const uint32_t indexed_flags = scene_node_category_bits(SceneNodeCategory::Renderable)
        | scene_node_category_bits(SceneNodeCategory::Pickable);
    for (const SceneNode& node : scene.m_nodes)
    {
        if (!scene_node_has_flags(node, indexed_flags))
        {
            continue;
        }

        insert_scene_node(scene, 0, node.m_id);
    }
}

Bounds3 marker_local_bounds()
{
    return Bounds3{
        .m_min = Float3{
            .m_x = -k_runtime_marker_half_extent,
            .m_y = -k_runtime_marker_half_extent,
            .m_z = k_default_scene_depth_min,
        },
        .m_max = Float3{
            .m_x = k_runtime_marker_half_extent,
            .m_y = k_runtime_marker_half_extent,
            .m_z = k_default_scene_depth_max,
        },
    };
}

uint32_t marker_flags_for_symbol(char symbol)
{
    const uint32_t renderable_pickable = scene_node_category_bits(SceneNodeCategory::Renderable)
        | scene_node_category_bits(SceneNodeCategory::Pickable);
    if (symbol == 'K' || symbol == 'S')
    {
        return renderable_pickable
            | scene_node_category_bits(SceneNodeCategory::InteractableAnchor);
    }

    return renderable_pickable
        | scene_node_category_bits(SceneNodeCategory::DynamicAttachment);
}

uint32_t marker_flags_for_entity(const DungeonMap::EntityPlacement& entity)
{
    uint32_t flags = marker_flags_for_symbol(entity.m_debug_symbol);
    if (dungeon_entity_blocks_physical(entity) && entity.m_movable)
    {
        flags |= scene_node_category_bits(SceneNodeCategory::BlocksMovement);
    }
    return flags;
}

uint32_t prefab_anchor_flags()
{
    return scene_node_category_bits(SceneNodeCategory::DynamicAttachment)
        | scene_node_category_bits(SceneNodeCategory::InteractableAnchor)
        | scene_node_category_bits(SceneNodeCategory::Renderable)
        | scene_node_category_bits(SceneNodeCategory::Pickable)
        | scene_node_category_bits(SceneNodeCategory::DebugOnly);
}

void update_scene_node_bounds_recursive(Scene& scene, SceneNodeId node_id)
{
    if (node_id == k_invalid_scene_node_id)
    {
        return;
    }

    SceneNode& node = scene.m_nodes[static_cast<std::size_t>(node_id - 1U)];
    node.m_world_bounds = translate_bounds(node.m_local_bounds, node.m_world_position);

    for (SceneNodeId child_id : node.m_child_ids)
    {
        SceneNode& child = scene.m_nodes[static_cast<std::size_t>(child_id - 1U)];
        child.m_world_position = add_float3(node.m_world_position, child.m_local_position);
        update_scene_node_bounds_recursive(scene, child_id);
    }
}

// NOTE: Scene query functions are not thread-safe. All queries must be
// called from the same thread that owns the Scene instance.
uint32_t begin_query_stamp(const Scene& scene)
{
    const std::size_t stamp_count = scene.m_nodes.size() + 1U;
    if (scene.m_query_stamps.size() < stamp_count)
    {
        scene.m_query_stamps.resize(stamp_count, 0U);
    }

    ++scene.m_query_generation;
    if (scene.m_query_generation == 0U)
    {
        std::fill(scene.m_query_stamps.begin(), scene.m_query_stamps.end(), 0U);
        scene.m_query_generation = 1U;
    }

    return scene.m_query_generation;
}

Bounds3 scene_root_bounds_for_map(const DungeonMap& map)
{
    const float width = static_cast<float>(map.m_width) * k_scene_tile_world_size;
    const float height = static_cast<float>(map.m_height) * k_scene_tile_world_size;
    return Bounds3{
        .m_min = Float3{.m_x = 0.0F, .m_y = 0.0F, .m_z = k_default_scene_depth_min},
        .m_max = Float3{.m_x = width, .m_y = height, .m_z = k_default_scene_depth_max},
    };
}

std::size_t chunk_index(int chunk_col, int chunk_row, int chunk_columns)
{
    return static_cast<std::size_t>(chunk_row * chunk_columns + chunk_col);
}

} // namespace

bool append_prefab_runtime_anchor_nodes(
    Scene& scene,
    const DungeonMap& map,
    std::vector<SceneNodeId>& out_node_ids);

bool build_scene_from_dungeon_map(const DungeonMap& map, Scene& out_scene)
{
    if (map.m_width <= 0 || map.m_height <= 0 || map.m_tiles.empty())
    {
        return false;
    }

    if (map.m_tiles.size()
        != static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height))
    {
        return false;
    }

    Scene scene{};
    const Bounds3 root_bounds = scene_root_bounds_for_map(map);
    scene.m_root_node_id = add_scene_node(
        scene,
        SceneNodeCreateInfo{
            .m_parent_id = k_invalid_scene_node_id,
            .m_local_position = Float3{},
            .m_local_bounds = root_bounds,
            .m_category_flags = scene_node_category_bits(SceneNodeCategory::StaticWorld),
        });

    if (scene.m_root_node_id == k_invalid_scene_node_id)
    {
        return false;
    }

    const int chunk_columns = (map.m_width + k_scene_chunk_tile_size - 1) / k_scene_chunk_tile_size;
    const int chunk_rows = (map.m_height + k_scene_chunk_tile_size - 1) / k_scene_chunk_tile_size;
    std::vector<SceneNodeId> chunk_nodes(
        static_cast<std::size_t>(chunk_columns * chunk_rows),
        k_invalid_scene_node_id);

    for (int chunk_row = 0; chunk_row < chunk_rows; ++chunk_row)
    {
        for (int chunk_col = 0; chunk_col < chunk_columns; ++chunk_col)
        {
            const int tiles_wide = std::min(k_scene_chunk_tile_size, map.m_width - (chunk_col * k_scene_chunk_tile_size));
            const int tiles_high = std::min(k_scene_chunk_tile_size, map.m_height - (chunk_row * k_scene_chunk_tile_size));

            const SceneNodeId chunk_node_id = add_scene_node(
                scene,
                SceneNodeCreateInfo{
                    .m_parent_id = scene.m_root_node_id,
                    .m_local_position = Float3{
                        .m_x = static_cast<float>(chunk_col * k_scene_chunk_tile_size)
                             * k_scene_tile_world_size,
                        .m_y = static_cast<float>(chunk_row * k_scene_chunk_tile_size)
                             * k_scene_tile_world_size,
                        .m_z = 0.0F,
                    },
                    .m_local_bounds = Bounds3{
                        .m_min = Float3{.m_x = 0.0F, .m_y = 0.0F, .m_z = k_default_scene_depth_min},
                        .m_max = Float3{
                            .m_x = static_cast<float>(tiles_wide) * k_scene_tile_world_size,
                            .m_y = static_cast<float>(tiles_high) * k_scene_tile_world_size,
                            .m_z = k_default_scene_depth_max,
                        },
                    },
                    .m_category_flags = scene_node_category_bits(SceneNodeCategory::StaticWorld),
                });

            chunk_nodes[chunk_index(chunk_col, chunk_row, chunk_columns)] = chunk_node_id;
        }
    }

    for (std::size_t tile_index = 0; tile_index < map.m_tiles.size(); ++tile_index)
    {
        const DungeonTile& tile = map.m_tiles[tile_index];
        const int chunk_col = tile.m_col / k_scene_chunk_tile_size;
        const int chunk_row = tile.m_row / k_scene_chunk_tile_size;
        const SceneNodeId parent_id = chunk_nodes[chunk_index(chunk_col, chunk_row, chunk_columns)];
        if (parent_id == k_invalid_scene_node_id)
        {
            return false;
        }

        const uint32_t node_flags = scene_node_category_bits(SceneNodeCategory::StaticWorld)
            | scene_node_category_bits(SceneNodeCategory::StaticGeometry)
            | scene_node_category_bits(SceneNodeCategory::Renderable)
            | scene_node_category_bits(SceneNodeCategory::Pickable);

        const SceneNodeId node_id = add_scene_node(
            scene,
            SceneNodeCreateInfo{
                .m_parent_id = parent_id,
                .m_local_position = Float3{
                    .m_x = static_cast<float>(tile.m_col % k_scene_chunk_tile_size)
                         * k_scene_tile_world_size,
                    .m_y = static_cast<float>(tile.m_row % k_scene_chunk_tile_size)
                         * k_scene_tile_world_size,
                    .m_z = 0.0F,
                },
                .m_local_bounds = Bounds3{
                    .m_min = Float3{.m_x = 0.0F, .m_y = 0.0F, .m_z = k_default_scene_depth_min},
                    .m_max = Float3{
                        .m_x = k_scene_tile_world_size,
                        .m_y = k_scene_tile_world_size,
                        .m_z = k_default_scene_depth_max,
                    },
                },
                .m_category_flags = node_flags,
                .m_payload_index = static_cast<int>(tile_index),
            });

        if (node_id == k_invalid_scene_node_id)
        {
            return false;
        }

    }

    for (const DungeonMap::EntityPlacement& entity : map.m_entity_placements)
    {
        if (entity.m_kind == DungeonMap::EntityKind::Unknown)
        {
            continue;
        }

        const Float3 marker_position{
            .m_x = (static_cast<float>(entity.m_col) + 0.5F) * k_scene_tile_world_size,
            .m_y = (static_cast<float>(entity.m_row) + 0.5F) * k_scene_tile_world_size,
            .m_z = 0.0F,
        };
        if (add_runtime_visual_node(
                scene,
                entity.m_debug_symbol,
                marker_position,
                marker_flags_for_entity(entity),
                -1)
            == k_invalid_scene_node_id)
        {
            return false;
        }
    }

    std::vector<SceneNodeId> prefab_anchor_node_ids{};
    if (!append_prefab_runtime_anchor_nodes(scene, map, prefab_anchor_node_ids))
    {
        return false;
    }

    rebuild_scene_spatial_index(scene, root_bounds);

    out_scene = std::move(scene);
    return true;
}

bool append_prefab_runtime_anchor_nodes(
    Scene& scene,
    const DungeonMap& map,
    std::vector<SceneNodeId>& out_node_ids)
{
    if (map.m_width <= 0 || map.m_height <= 0)
    {
        return false;
    }

    for (std::size_t i = 0; i < map.m_prefab_placements.size(); ++i)
    {
        const DungeonMap::PrefabPlacement& prefab = map.m_prefab_placements[i];
        if (prefab.m_prefab_id == 0U || prefab.m_width <= 0 || prefab.m_height <= 0)
        {
            return false;
        }

        const int max_col = prefab.m_origin_col + prefab.m_width;
        const int max_row = prefab.m_origin_row + prefab.m_height;
        if (prefab.m_origin_col < 0
            || prefab.m_origin_row < 0
            || max_col > map.m_width
            || max_row > map.m_height)
        {
            return false;
        }

        const Float3 anchor_world{
            .m_x = (static_cast<float>(prefab.m_origin_col) + (static_cast<float>(prefab.m_width) * 0.5F))
                 * k_scene_tile_world_size,
            .m_y = (static_cast<float>(prefab.m_origin_row) + (static_cast<float>(prefab.m_height) * 0.5F))
                 * k_scene_tile_world_size,
            .m_z = 0.0F,
        };

        const SceneNodeId node_id = add_scene_node(
            scene,
            SceneNodeCreateInfo{
                .m_parent_id = scene.m_root_node_id,
                .m_local_position = anchor_world,
                .m_local_bounds = marker_local_bounds(),
                .m_category_flags = prefab_anchor_flags(),
                .m_payload_index = static_cast<int>(i),
                .m_debug_symbol = 'F',
            });
        if (node_id == k_invalid_scene_node_id)
        {
            return false;
        }

        out_node_ids.push_back(node_id);
    }

    return true;
}

SceneNodeId add_runtime_visual_node(
    Scene& scene,
    char symbol,
    const Float3& world_position,
    uint32_t category_flags,
    int payload_index)
{
    if (scene.m_root_node_id == k_invalid_scene_node_id)
    {
        return k_invalid_scene_node_id;
    }

    const SceneNodeId node_id = add_scene_node(
        scene,
        SceneNodeCreateInfo{
            .m_parent_id = scene.m_root_node_id,
            .m_local_position = world_position,
            .m_local_bounds = marker_local_bounds(),
            .m_category_flags = category_flags,
            .m_payload_index = payload_index,
            .m_debug_symbol = symbol,
        });
    if (node_id == k_invalid_scene_node_id)
    {
        return node_id;
    }

    rebuild_scene_spatial_index(scene, scene.m_spatial_index.m_root_bounds);
    return node_id;
}

SceneNodeId add_runtime_marker_node(Scene& scene, char symbol, const Float3& world_position)
{
    return add_runtime_visual_node(scene, symbol, world_position, marker_flags_for_symbol(symbol), -1);
}

bool update_scene_node_world_position(Scene& scene, SceneNodeId node_id, const Float3& world_position)
{
    if (node_id == k_invalid_scene_node_id)
    {
        return false;
    }

    const std::size_t node_index = static_cast<std::size_t>(node_id - 1U);
    if (node_index >= scene.m_nodes.size())
    {
        return false;
    }

    SceneNode& node = scene.m_nodes[node_index];
    node.m_world_position = world_position;
    if (node.m_parent_id == k_invalid_scene_node_id)
    {
        node.m_local_position = world_position;
    }
    else
    {
        const SceneNode* parent = find_scene_node(scene, node.m_parent_id);
        if (parent == nullptr)
        {
            return false;
        }

        node.m_local_position = Float3{
            .m_x = world_position.m_x - parent->m_world_position.m_x,
            .m_y = world_position.m_y - parent->m_world_position.m_y,
            .m_z = world_position.m_z - parent->m_world_position.m_z,
        };
    }

    update_scene_node_bounds_recursive(scene, node_id);
    rebuild_scene_spatial_index(scene, scene.m_spatial_index.m_root_bounds);
    return true;
}

const SceneNode* find_scene_node(const Scene& scene, SceneNodeId node_id)
{
    if (node_id == k_invalid_scene_node_id)
    {
        return nullptr;
    }

    const std::size_t node_index = static_cast<std::size_t>(node_id - 1U);
    if (node_index >= scene.m_nodes.size())
    {
        return nullptr;
    }

    return &scene.m_nodes[node_index];
}

SceneNodeId find_first_scene_node_by_symbol(const Scene& scene, char symbol)
{
    for (const SceneNode& node : scene.m_nodes)
    {
        if (node.m_debug_symbol == symbol)
        {
            return node.m_id;
        }
    }

    return k_invalid_scene_node_id;
}

std::vector<SceneNodeId> query_scene_bounds(const Scene& scene, const Bounds3& bounds)
{
    std::vector<SceneNodeId> out_node_ids{};
    if (scene.m_spatial_index.m_cells.empty())
    {
        return out_node_ids;
    }

    const uint32_t current_gen = begin_query_stamp(scene);
    std::vector<int> pending_cells{0};
    while (!pending_cells.empty())
    {
        const int cell_index = pending_cells.back();
        pending_cells.pop_back();
        if (cell_index < 0 || static_cast<std::size_t>(cell_index) >= scene.m_spatial_index.m_cells.size())
        {
            continue;
        }

        const SceneOctreeCell& cell = scene.m_spatial_index.m_cells[static_cast<std::size_t>(cell_index)];
        if (!bounds_intersect(cell.m_query_bounds, bounds))
        {
            continue;
        }

        for (SceneNodeId node_id : cell.m_node_ids)
        {
            const std::size_t stamp_index = static_cast<std::size_t>(node_id);
            if (stamp_index >= scene.m_query_stamps.size()
                || scene.m_query_stamps[stamp_index] == current_gen)
            {
                continue;
            }

            const SceneNode* node = find_scene_node(scene, node_id);
            if (node != nullptr && bounds_intersect(node->m_world_bounds, bounds))
            {
                scene.m_query_stamps[stamp_index] = current_gen;
                out_node_ids.push_back(node_id);
            }
        }

        for (int child_index : cell.m_child_indices)
        {
            if (child_index >= 0)
            {
                pending_cells.push_back(child_index);
            }
        }
    }

    return out_node_ids;
}

bool any_scene_node_blocks_bounds(
    const Scene& scene,
    const Bounds3& bounds,
    SceneNodeId ignored_node_id,
    uint32_t blocking_flags)
{
    const std::vector<SceneNodeId> candidate_node_ids = query_scene_bounds(scene, bounds);
    for (SceneNodeId node_id : candidate_node_ids)
    {
        if (node_id == ignored_node_id)
        {
            continue;
        }

        const SceneNode* node = find_scene_node(scene, node_id);
        if (node == nullptr)
        {
            continue;
        }

        if (blocking_flags != 0U && !scene_node_has_flags(*node, blocking_flags))
        {
            continue;
        }

        return true;
    }

    return false;
}

std::vector<SceneNodeId> query_scene_point(
    const Scene& scene,
    const Float3& world_point,
    uint32_t required_flags)
{
    std::vector<SceneNodeId> out_node_ids{};
    if (scene.m_spatial_index.m_cells.empty())
    {
        return out_node_ids;
    }

    const uint32_t current_gen = begin_query_stamp(scene);
    std::vector<int> pending_cells{0};
    while (!pending_cells.empty())
    {
        const int cell_index = pending_cells.back();
        pending_cells.pop_back();
        if (cell_index < 0 || static_cast<std::size_t>(cell_index) >= scene.m_spatial_index.m_cells.size())
        {
            continue;
        }

        const SceneOctreeCell& cell = scene.m_spatial_index.m_cells[static_cast<std::size_t>(cell_index)];
        if (!point_in_bounds(world_point, cell.m_query_bounds))
        {
            continue;
        }

        for (SceneNodeId node_id : cell.m_node_ids)
        {
            const std::size_t stamp_index = static_cast<std::size_t>(node_id);
            if (stamp_index >= scene.m_query_stamps.size()
                || scene.m_query_stamps[stamp_index] == current_gen)
            {
                continue;
            }

            const SceneNode* node = find_scene_node(scene, node_id);
            if (node == nullptr)
            {
                continue;
            }

            if (required_flags != 0U && !scene_node_has_flags(*node, required_flags))
            {
                continue;
            }

            if (point_in_bounds(world_point, node->m_world_bounds))
            {
                scene.m_query_stamps[stamp_index] = current_gen;
                out_node_ids.push_back(node_id);
            }
        }

        for (int child_index : cell.m_child_indices)
        {
            if (child_index >= 0)
            {
                pending_cells.push_back(child_index);
            }
        }
    }

    return out_node_ids;
}

SceneNodeId pick_scene_node_at_point(
    const Scene& scene,
    const Float3& world_point,
    uint32_t required_flags)
{
    const std::vector<SceneNodeId> candidates =
        query_scene_point(scene, world_point, required_flags);

    SceneNodeId best_id = k_invalid_scene_node_id;
    float best_area = std::numeric_limits<float>::max();

    for (SceneNodeId node_id : candidates)
    {
        const SceneNode* node = find_scene_node(scene, node_id);
        if (node == nullptr)
        {
            continue;
        }

        const float width = node->m_world_bounds.m_max.m_x - node->m_world_bounds.m_min.m_x;
        const float height = node->m_world_bounds.m_max.m_y - node->m_world_bounds.m_min.m_y;
        const float area = width * height;
        if (area < best_area || (area == best_area && node_id < best_id))
        {
            best_id = node_id;
            best_area = area;
        }
    }

    return best_id;
}

SceneDebugMetrics collect_scene_debug_metrics(const Scene& scene)
{
    SceneDebugMetrics metrics{};
    metrics.m_cell_count = scene.m_spatial_index.m_cells.size();
    metrics.m_indexed_node_count = scene.m_spatial_index.m_indexed_node_count;

    for (const SceneOctreeCell& cell : scene.m_spatial_index.m_cells)
    {
        metrics.m_max_depth = std::max(metrics.m_max_depth, cell.m_depth);

        bool has_child = false;
        for (int child_index : cell.m_child_indices)
        {
            if (child_index >= 0)
            {
                has_child = true;
                break;
            }
        }

        if (!has_child)
        {
            ++metrics.m_leaf_cell_count;
        }
    }

    return metrics;
}

bool scene_node_has_flags(const SceneNode& node, uint32_t required_flags)
{
    return (node.m_category_flags & required_flags) == required_flags;
}

} // namespace mordor