#include "mordor/world_mesh.hpp"

#include "mordor/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace mordor {

namespace {

struct TileGeomInput
{
    float m_x0{0.0F};
    float m_z0{0.0F};
    float m_x1{0.0F};
    float m_z1{0.0F};
    bool  m_is_wall{false};
    char  m_symbol{'.'};
    int   m_sort_key{0}; // payload_index = row * width + col
};

enum class WallMaterial : uint8_t
{
    None = 0,
    Stone,
    Door,
};

struct WallPalette
{
    float m_top_r{0.0F};
    float m_top_g{0.0F};
    float m_top_b{0.0F};
    float m_primary_r{0.0F};
    float m_primary_g{0.0F};
    float m_primary_b{0.0F};
    float m_secondary_r{0.0F};
    float m_secondary_g{0.0F};
    float m_secondary_b{0.0F};
};

WallPalette wall_palette(WallMaterial material)
{
    if (material == WallMaterial::Door)
    {
        return WallPalette{
            .m_top_r = 0.52F,
            .m_top_g = 0.34F,
            .m_top_b = 0.18F,
            .m_primary_r = 0.43F,
            .m_primary_g = 0.27F,
            .m_primary_b = 0.14F,
            .m_secondary_r = 0.35F,
            .m_secondary_g = 0.22F,
            .m_secondary_b = 0.12F,
        };
    }

    return WallPalette{
        .m_top_r = 0.62F,
        .m_top_g = 0.20F,
        .m_top_b = 0.20F,
        .m_primary_r = 0.50F,
        .m_primary_g = 0.16F,
        .m_primary_b = 0.16F,
        .m_secondary_r = 0.40F,
        .m_secondary_g = 0.13F,
        .m_secondary_b = 0.13F,
    };
}

void emit_quad(
    WorldMesh&  mesh,
    std::vector<uint32_t>& index_buffer,
    WorldVertex v0,
    WorldVertex v1,
    WorldVertex v2,
    WorldVertex v3)
{
    const auto base = static_cast<uint32_t>(mesh.m_vertices.size());
    mesh.m_vertices.push_back(v0);
    mesh.m_vertices.push_back(v1);
    mesh.m_vertices.push_back(v2);
    mesh.m_vertices.push_back(v3);

    index_buffer.push_back(base + 0U);
    index_buffer.push_back(base + 1U);
    index_buffer.push_back(base + 2U);
    index_buffer.push_back(base + 0U);
    index_buffer.push_back(base + 2U);
    index_buffer.push_back(base + 3U);

    // Keep combined index stream for diagnostics/tests.
    mesh.m_indices.push_back(base + 0U);
    mesh.m_indices.push_back(base + 1U);
    mesh.m_indices.push_back(base + 2U);
    mesh.m_indices.push_back(base + 0U);
    mesh.m_indices.push_back(base + 2U);
    mesh.m_indices.push_back(base + 3U);
}

void emit_floor_tile(WorldMesh& mesh, std::vector<uint32_t>& index_buffer, float x0, float z0, float x1, float z1)
{
    constexpr float r = 0.18F;
    constexpr float g = 0.43F;
    constexpr float b = 0.24F;
    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x0, 0.0F, z0, r, g, b},
        WorldVertex{x1, 0.0F, z0, r, g, b},
        WorldVertex{x1, 0.0F, z1, r, g, b},
        WorldVertex{x0, 0.0F, z1, r, g, b});
}

void emit_wall_top_face(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float x0,
    float z0,
    float x1,
    float z1,
    float alpha,
    WallMaterial material)
{
    const float h = k_wall_height;
    const WallPalette p = wall_palette(material);

    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x0, h, z0, p.m_top_r, p.m_top_g, p.m_top_b, alpha},
        WorldVertex{x1, h, z0, p.m_top_r, p.m_top_g, p.m_top_b, alpha},
        WorldVertex{x1, h, z1, p.m_top_r, p.m_top_g, p.m_top_b, alpha},
        WorldVertex{x0, h, z1, p.m_top_r, p.m_top_g, p.m_top_b, alpha});
}

void emit_wall_south_face(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float x0,
    float z,
    float x1,
    float alpha,
    WallMaterial material)
{
    const float h = k_wall_height;
    const WallPalette p = wall_palette(material);

    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x0, 0.0F, z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x1, 0.0F, z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x1, h,    z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x0, h,    z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha});
}

void emit_wall_north_face(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float x0,
    float z,
    float x1,
    float alpha,
    WallMaterial material)
{
    const float h = k_wall_height;
    const WallPalette p = wall_palette(material);

    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x1, 0.0F, z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x0, 0.0F, z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x0, h,    z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha},
        WorldVertex{x1, h,    z, p.m_primary_r, p.m_primary_g, p.m_primary_b, alpha});
}

void emit_wall_east_face(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float x,
    float z0,
    float z1,
    float alpha,
    WallMaterial material)
{
    const float h = k_wall_height;
    const WallPalette p = wall_palette(material);

    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x, 0.0F, z0, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, 0.0F, z1, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, h,    z1, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, h,    z0, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha});
}

void emit_wall_west_face(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float x,
    float z0,
    float z1,
    float alpha,
    WallMaterial material)
{
    const float h = k_wall_height;
    const WallPalette p = wall_palette(material);

    emit_quad(
        mesh,
        index_buffer,
        WorldVertex{x, 0.0F, z1, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, 0.0F, z0, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, h,    z0, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha},
        WorldVertex{x, h,    z1, p.m_secondary_r, p.m_secondary_g, p.m_secondary_b, alpha});
}

std::size_t tile_index_for(int width, int col, int row)
{
    return static_cast<std::size_t>((row * width) + col);
}

bool tile_in_bounds(const DungeonMap& map, int col, int row)
{
    return col >= 0 && row >= 0 && col < map.m_width && row < map.m_height;
}

WallMaterial wall_material_at(const DungeonMap& map, int col, int row)
{
    if (!tile_in_bounds(map, col, row))
    {
        return WallMaterial::None;
    }

    const std::size_t idx = tile_index_for(map.m_width, col, row);
    if (idx >= map.m_tiles.size() || !map.m_tiles[idx].m_blocks_movement)
    {
        return WallMaterial::None;
    }

    return map.m_tiles[idx].m_symbol == 'D' ? WallMaterial::Door : WallMaterial::Stone;
}
void emit_merged_walls(
    WorldMesh& mesh,
    const DungeonMap& map)
{
    const int width = map.m_width;
    const int height = map.m_height;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const std::size_t tile_count = static_cast<std::size_t>(width * height);
    std::vector<WallMaterial> materials(tile_count, WallMaterial::None);

    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            const std::size_t idx = tile_index_for(width, col, row);
            materials[idx] = wall_material_at(map, col, row);
        }
    }

    const auto material_at = [&materials, width, height](int col, int row) {
        if (col < 0 || row < 0 || col >= width || row >= height)
        {
            return WallMaterial::None;
        }
        return materials[tile_index_for(width, col, row)];
    };

    std::vector<uint8_t> top_visited(tile_count, 0U);
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            const std::size_t root_idx = tile_index_for(width, col, row);
            const WallMaterial root_material = materials[root_idx];
            if (root_material == WallMaterial::None || top_visited[root_idx] != 0U)
            {
                continue;
            }

            int run_width = 1;
            while ((col + run_width) < width)
            {
                const std::size_t idx = tile_index_for(width, col + run_width, row);
                if (top_visited[idx] != 0U || materials[idx] != root_material)
                {
                    break;
                }
                ++run_width;
            }

            int run_height = 1;
            bool can_extend = true;
            while ((row + run_height) < height && can_extend)
            {
                for (int offset = 0; offset < run_width; ++offset)
                {
                    const std::size_t idx = tile_index_for(width, col + offset, row + run_height);
                    if (top_visited[idx] != 0U || materials[idx] != root_material)
                    {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend)
                {
                    ++run_height;
                }
            }

            for (int r = 0; r < run_height; ++r)
            {
                for (int c = 0; c < run_width; ++c)
                {
                    const std::size_t idx = tile_index_for(width, col + c, row + r);
                    top_visited[idx] = 1U;
                }
            }

            std::vector<uint32_t>& index_buffer = mesh.m_transparent_indices;

            const float x0 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(row) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col + run_width) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row + run_height) * k_scene_tile_world_size;
            emit_wall_top_face(mesh, index_buffer, x0, z0, x1, z1, 1.0F, root_material);
        }
    }

    for (int row = 0; row < height; ++row)
    {
        int col = 0;
        while (col < width)
        {
            const WallMaterial material = material_at(col, row);
            if (material == WallMaterial::None || material_at(col, row - 1) != WallMaterial::None)
            {
                ++col;
                continue;
            }

            int run_start = col;
            ++col;
            while (col < width
                && material_at(col, row) == material
                && material_at(col, row - 1) == WallMaterial::None)
            {
                ++col;
            }

            std::vector<uint32_t>& index_buffer = mesh.m_transparent_indices;
            const float x0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_north_face(mesh, index_buffer, x0, z, x1, 1.0F, material);
        }

        col = 0;
        while (col < width)
        {
            const WallMaterial material = material_at(col, row);
            if (material == WallMaterial::None || material_at(col, row + 1) != WallMaterial::None)
            {
                ++col;
                continue;
            }

            int run_start = col;
            ++col;
            while (col < width
                && material_at(col, row) == material
                && material_at(col, row + 1) == WallMaterial::None)
            {
                ++col;
            }

            std::vector<uint32_t>& index_buffer = mesh.m_transparent_indices;
            const float x0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z = static_cast<float>(row + 1) * k_scene_tile_world_size;
            emit_wall_south_face(mesh, index_buffer, x0, z, x1, 1.0F, material);
        }
    }

    for (int col = 0; col < width; ++col)
    {
        int row = 0;
        while (row < height)
        {
            const WallMaterial material = material_at(col, row);
            if (material == WallMaterial::None || material_at(col + 1, row) != WallMaterial::None)
            {
                ++row;
                continue;
            }

            int run_start = row;
            ++row;
            while (row < height
                && material_at(col, row) == material
                && material_at(col + 1, row) == WallMaterial::None)
            {
                ++row;
            }

            std::vector<uint32_t>& index_buffer = mesh.m_transparent_indices;
            const float x = static_cast<float>(col + 1) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_east_face(mesh, index_buffer, x, z0, z1, 1.0F, material);
        }

        row = 0;
        while (row < height)
        {
            const WallMaterial material = material_at(col, row);
            if (material == WallMaterial::None || material_at(col - 1, row) != WallMaterial::None)
            {
                ++row;
                continue;
            }

            int run_start = row;
            ++row;
            while (row < height
                && material_at(col, row) == material
                && material_at(col - 1, row) == WallMaterial::None)
            {
                ++row;
            }

            std::vector<uint32_t>& index_buffer = mesh.m_transparent_indices;
            const float x = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_west_face(mesh, index_buffer, x, z0, z1, 1.0F, material);
        }
    }
}

} // namespace

WorldMesh build_world_mesh(
    const Scene& scene,
    const DungeonMap& map,
    float camera_x,
    float camera_z,
    float anchor_x,
    float anchor_z)
{
    (void)camera_x;
    (void)camera_z;
    (void)anchor_x;
    (void)anchor_z;

    WorldMesh mesh{};

    if (map.m_tiles.empty() || map.m_width <= 0 || map.m_height <= 0)
    {
        return mesh;
    }

    const uint32_t renderable_static_flags =
        scene_node_category_bits(SceneNodeCategory::StaticGeometry)
        | scene_node_category_bits(SceneNodeCategory::Renderable);

    std::vector<TileGeomInput> tile_inputs{};
    tile_inputs.reserve(map.m_tiles.size());

    for (const SceneNode& node : scene.m_nodes)
    {
        if (!scene_node_has_flags(node, renderable_static_flags) || node.m_payload_index < 0)
        {
            continue;
        }

        const auto tile_index = static_cast<std::size_t>(node.m_payload_index);
        if (tile_index >= map.m_tiles.size())
        {
            continue;
        }

        const DungeonTile& tile = map.m_tiles[tile_index];
        tile_inputs.push_back(TileGeomInput{
            .m_x0 = node.m_world_bounds.m_min.m_x,
            .m_z0 = node.m_world_bounds.m_min.m_y,
            .m_x1 = node.m_world_bounds.m_max.m_x,
            .m_z1 = node.m_world_bounds.m_max.m_y,
            .m_is_wall  = tile.m_blocks_movement,
            .m_symbol = tile.m_symbol,
            .m_sort_key = node.m_payload_index,
        });
    }

    std::sort(
        tile_inputs.begin(),
        tile_inputs.end(),
        [](const TileGeomInput& a, const TileGeomInput& b) {
            return a.m_sort_key < b.m_sort_key;
        });

    mesh.m_vertices.reserve(tile_inputs.size() * 12);
    mesh.m_indices.reserve(tile_inputs.size() * 18);
    mesh.m_opaque_indices.reserve(tile_inputs.size() * 16);
    mesh.m_transparent_indices.reserve(tile_inputs.size() * 8);

    emit_merged_walls(mesh, map);

    for (const TileGeomInput& t : tile_inputs)
    {
        if (t.m_is_wall)
        {
            continue;
        }

        emit_floor_tile(mesh, mesh.m_opaque_indices, t.m_x0, t.m_z0, t.m_x1, t.m_z1);
    }

    return mesh;
}

namespace {

bool bounds_overlap(const Bounds3& a, const Bounds3& b)
{
    return a.m_min.m_x <= b.m_max.m_x && a.m_max.m_x >= b.m_min.m_x
        && a.m_min.m_y <= b.m_max.m_y && a.m_max.m_y >= b.m_min.m_y
        && a.m_min.m_z <= b.m_max.m_z && a.m_max.m_z >= b.m_min.m_z;
}

Bounds3 combine_bounds(const Bounds3& a, const Bounds3& b)
{
    return Bounds3{
        .m_min = Float3{
            .m_x = std::min(a.m_min.m_x, b.m_min.m_x),
            .m_y = std::min(a.m_min.m_y, b.m_min.m_y),
            .m_z = std::min(a.m_min.m_z, b.m_min.m_z),
        },
        .m_max = Float3{
            .m_x = std::max(a.m_max.m_x, b.m_max.m_x),
            .m_y = std::max(a.m_max.m_y, b.m_max.m_y),
            .m_z = std::max(a.m_max.m_z, b.m_max.m_z),
        },
    };
}

void build_octree_node(
    WallCollisionOctree& octree,
    const std::vector<Bounds3>& surface_bounds,
    const std::vector<int>& surface_indices,
    const Bounds3& node_bounds,
    int depth,
    int max_depth,
    int max_surfaces_per_node,
    int& out_node_index)
{
    out_node_index = static_cast<int>(octree.m_nodes.size());
    octree.m_nodes.push_back(WallCollisionOctreeNode{});
    WallCollisionOctreeNode& node = octree.m_nodes.back();
    node.m_bounds = node_bounds;

    if (depth >= max_depth || static_cast<int>(surface_indices.size()) <= max_surfaces_per_node)
    {
        node.m_surface_indices = surface_indices;
        return;
    }

    const Float3 center{
        .m_x = (node_bounds.m_min.m_x + node_bounds.m_max.m_x) * 0.5F,
        .m_y = (node_bounds.m_min.m_y + node_bounds.m_max.m_y) * 0.5F,
        .m_z = (node_bounds.m_min.m_z + node_bounds.m_max.m_z) * 0.5F,
    };

    int child_count = 0;
    for (int child = 0; child < 8; ++child)
    {
        const bool high_x = (child & 1) != 0;
        const bool high_y = (child & 2) != 0;
        const bool high_z = (child & 4) != 0;

        const Bounds3 child_bounds{
            .m_min = Float3{
                .m_x = high_x ? center.m_x : node_bounds.m_min.m_x,
                .m_y = high_y ? center.m_y : node_bounds.m_min.m_y,
                .m_z = high_z ? center.m_z : node_bounds.m_min.m_z,
            },
            .m_max = Float3{
                .m_x = high_x ? node_bounds.m_max.m_x : center.m_x,
                .m_y = high_y ? node_bounds.m_max.m_y : center.m_y,
                .m_z = high_z ? node_bounds.m_max.m_z : center.m_z,
            },
        };

        std::vector<int> child_surfaces{};
        child_surfaces.reserve(surface_indices.size());
        for (int surface_index : surface_indices)
        {
            if (bounds_overlap(surface_bounds[static_cast<std::size_t>(surface_index)], child_bounds))
            {
                child_surfaces.push_back(surface_index);
            }
        }

        if (child_surfaces.empty())
        {
            continue;
        }

        int child_index = -1;
        build_octree_node(
            octree,
            surface_bounds,
            child_surfaces,
            child_bounds,
            depth + 1,
            max_depth,
            max_surfaces_per_node,
            child_index);
        node.m_children.push_back(child_index);
        ++child_count;
    }

    if (child_count == 0)
    {
        node.m_surface_indices = surface_indices;
    }
}

} // namespace

bool build_wall_collision_octree(const DungeonMap& map, WallCollisionOctree& out_octree)
{
    out_octree = WallCollisionOctree{};
    if (map.m_width <= 0 || map.m_height <= 0 || map.m_tiles.empty())
    {
        return false;
    }

    const int width = map.m_width;
    const int height = map.m_height;
    const std::size_t tile_count = static_cast<std::size_t>(width * height);
    std::vector<uint8_t> blocked(tile_count, 0U);
    std::vector<uint8_t> visited(tile_count, 0U);

    for (const DungeonTile& tile : map.m_tiles)
    {
        if (!tile.m_blocks_movement || tile.m_col < 0 || tile.m_row < 0
            || tile.m_col >= width || tile.m_row >= height)
        {
            continue;
        }
        const std::size_t idx = static_cast<std::size_t>((tile.m_row * width) + tile.m_col);
        if (idx < blocked.size())
        {
            blocked[idx] = 1U;
        }
    }

    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            const std::size_t idx = static_cast<std::size_t>((row * width) + col);
            if (blocked[idx] == 0U || visited[idx] != 0U)
            {
                continue;
            }

            int run_width = 1;
            while ((col + run_width) < width)
            {
                const std::size_t probe = static_cast<std::size_t>((row * width) + (col + run_width));
                if (blocked[probe] == 0U || visited[probe] != 0U)
                {
                    break;
                }
                ++run_width;
            }

            int run_height = 1;
            bool can_extend = true;
            while ((row + run_height) < height && can_extend)
            {
                for (int offset = 0; offset < run_width; ++offset)
                {
                    const std::size_t probe = static_cast<std::size_t>(((row + run_height) * width) + (col + offset));
                    if (blocked[probe] == 0U || visited[probe] != 0U)
                    {
                        can_extend = false;
                        break;
                    }
                }
                if (can_extend)
                {
                    ++run_height;
                }
            }

            for (int r = 0; r < run_height; ++r)
            {
                for (int c = 0; c < run_width; ++c)
                {
                    const std::size_t mark = static_cast<std::size_t>(((row + r) * width) + (col + c));
                    visited[mark] = 1U;
                }
            }

            const float x0 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(row) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col + run_width) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row + run_height) * k_scene_tile_world_size;

            out_octree.m_surfaces.push_back(WallSurfaceBounds{
                .m_bounds = Bounds3{
                    .m_min = Float3{.m_x = x0, .m_y = 0.0F, .m_z = z0},
                    .m_max = Float3{.m_x = x1, .m_y = k_wall_height, .m_z = z1},
                },
            });
        }
    }

    if (out_octree.m_surfaces.empty())
    {
        return false;
    }

    Bounds3 root_bounds{
        .m_min = Float3{
            .m_x = std::numeric_limits<float>::max(),
            .m_y = std::numeric_limits<float>::max(),
            .m_z = std::numeric_limits<float>::max(),
        },
        .m_max = Float3{
            .m_x = std::numeric_limits<float>::lowest(),
            .m_y = std::numeric_limits<float>::lowest(),
            .m_z = std::numeric_limits<float>::lowest(),
        },
    };

    std::vector<Bounds3> surface_bounds{};
    surface_bounds.reserve(out_octree.m_surfaces.size());
    std::vector<int> surface_indices{};
    surface_indices.reserve(out_octree.m_surfaces.size());
    for (std::size_t i = 0; i < out_octree.m_surfaces.size(); ++i)
    {
        const Bounds3& b = out_octree.m_surfaces[i].m_bounds;
        surface_bounds.push_back(b);
        surface_indices.push_back(static_cast<int>(i));
        root_bounds = combine_bounds(root_bounds, b);
    }

    int root_node = -1;
    build_octree_node(
        out_octree,
        surface_bounds,
        surface_indices,
        root_bounds,
        0,
        4,
        12,
        root_node);

    return root_node == 0 && !out_octree.m_nodes.empty();
}

bool wall_collision_octree_overlaps_bounds(const WallCollisionOctree& octree, const Bounds3& bounds)
{
    if (octree.m_nodes.empty())
    {
        return false;
    }

    std::vector<int> pending_nodes{0};
    while (!pending_nodes.empty())
    {
        const int node_index = pending_nodes.back();
        pending_nodes.pop_back();
        if (node_index < 0 || static_cast<std::size_t>(node_index) >= octree.m_nodes.size())
        {
            continue;
        }

        const WallCollisionOctreeNode& node = octree.m_nodes[static_cast<std::size_t>(node_index)];
        if (!bounds_overlap(node.m_bounds, bounds))
        {
            continue;
        }

        for (int surface_index : node.m_surface_indices)
        {
            if (surface_index < 0 || static_cast<std::size_t>(surface_index) >= octree.m_surfaces.size())
            {
                continue;
            }

            if (bounds_overlap(octree.m_surfaces[static_cast<std::size_t>(surface_index)].m_bounds, bounds))
            {
                return true;
            }
        }

        for (int child_index : node.m_children)
        {
            pending_nodes.push_back(child_index);
        }
    }

    return false;
}

} // namespace mordor
