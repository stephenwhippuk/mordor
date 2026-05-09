#include "mordor/world_mesh.hpp"

#include "mordor/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

bool segment_intersects_rect_2d(
    float x0,
    float z0,
    float x1,
    float z1,
    float min_x,
    float min_z,
    float max_x,
    float max_z)
{
    float t0 = 0.0F;
    float t1 = 1.0F;

    const float dx = x1 - x0;
    const float dz = z1 - z0;

    const auto clip = [&t0, &t1](float p, float q) {
        if (std::fabs(p) <= 1.0e-6F)
        {
            return q >= 0.0F;
        }

        const float r = q / p;
        if (p < 0.0F)
        {
            if (r > t1)
            {
                return false;
            }
            if (r > t0)
            {
                t0 = r;
            }
        }
        else
        {
            if (r < t0)
            {
                return false;
            }
            if (r < t1)
            {
                t1 = r;
            }
        }

        return true;
    };

    return clip(-dx, x0 - min_x)
        && clip(dx, max_x - x0)
        && clip(-dz, z0 - min_z)
        && clip(dz, max_z - z0);
}

float calculate_occlusion_alpha(
    float wall_x0,
    float wall_z0,
    float wall_x1,
    float wall_z1,
    float camera_x,
    float camera_z,
    float anchor_x,
    float anchor_z)
{
    const float wall_cx = (wall_x0 + wall_x1) * 0.5F;
    const float wall_cz = (wall_z0 + wall_z1) * 0.5F;

    constexpr float occluder_padding = 2.0F;
    if (!segment_intersects_rect_2d(
            camera_x,
            camera_z,
            anchor_x,
            anchor_z,
            wall_x0 - occluder_padding,
            wall_z0 - occluder_padding,
            wall_x1 + occluder_padding,
            wall_z1 + occluder_padding))
    {
        return 1.0F;
    }

    const float seg_dx = anchor_x - camera_x;
    const float seg_dz = anchor_z - camera_z;
    const float seg_len_sq = (seg_dx * seg_dx) + (seg_dz * seg_dz);
    if (seg_len_sq <= 1.0e-4F)
    {
        return 1.0F;
    }

    const float cam_to_wall_x = wall_cx - camera_x;
    const float cam_to_wall_z = wall_cz - camera_z;
    const float projected = ((cam_to_wall_x * seg_dx) + (cam_to_wall_z * seg_dz)) / seg_len_sq;
    if (projected <= 0.0F || projected >= 1.0F)
    {
        return 1.0F;
    }

    const float closest_x = camera_x + (projected * seg_dx);
    const float closest_z = camera_z + (projected * seg_dz);
    const float lateral_dx = wall_cx - closest_x;
    const float lateral_dz = wall_cz - closest_z;
    const float lateral_dist = std::sqrt((lateral_dx * lateral_dx) + (lateral_dz * lateral_dz));

    constexpr float corridor_half_width = k_scene_tile_world_size * 0.75F;
    if (lateral_dist >= corridor_half_width)
    {
        return 1.0F;
    }

    const float to_anchor_dx = wall_cx - anchor_x;
    const float to_anchor_dz = wall_cz - anchor_z;
    const float dist_to_anchor = std::sqrt((to_anchor_dx * to_anchor_dx) + (to_anchor_dz * to_anchor_dz));
    constexpr float occlusion_radius = k_scene_tile_world_size * 4.0F;
    if (dist_to_anchor > occlusion_radius)
    {
        return 1.0F;
    }

    const float corridor_factor = 1.0F - std::clamp(lateral_dist / corridor_half_width, 0.0F, 1.0F);
    const float radius_factor = 1.0F - std::clamp(dist_to_anchor / occlusion_radius, 0.0F, 1.0F);
    const float fade_strength = std::clamp(corridor_factor * radius_factor, 0.0F, 1.0F);

    constexpr float min_alpha = 0.30F;
    constexpr float max_alpha = 1.00F;
    return max_alpha - ((max_alpha - min_alpha) * fade_strength);
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

int alpha_bucket(float alpha)
{
    return (alpha < 0.999F) ? 1 : 0;
}

void emit_marker_sphere(
    WorldMesh& mesh,
    std::vector<uint32_t>& index_buffer,
    float center_x,
    float center_z,
    float radius,
    float r,
    float g,
    float b)
{
    constexpr int stacks = 6;
    constexpr int slices = 10;
    const float center_y = radius + 2.0F;

    const auto base = static_cast<uint32_t>(mesh.m_vertices.size());
    for (int stack = 0; stack <= stacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = v * 3.1415926535F;
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        for (int slice = 0; slice <= slices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * 6.283185307F;
            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);

            mesh.m_vertices.push_back(WorldVertex{
                center_x + radius * sin_phi * cos_theta,
                center_y + radius * cos_phi,
                center_z + radius * sin_phi * sin_theta,
                r,
                g,
                b,
                1.0F,
            });
        }
    }

    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            const uint32_t row0 = static_cast<uint32_t>(stack * (slices + 1));
            const uint32_t row1 = static_cast<uint32_t>((stack + 1) * (slices + 1));
            const uint32_t i0 = base + row0 + static_cast<uint32_t>(slice);
            const uint32_t i1 = base + row0 + static_cast<uint32_t>(slice + 1);
            const uint32_t i2 = base + row1 + static_cast<uint32_t>(slice);
            const uint32_t i3 = base + row1 + static_cast<uint32_t>(slice + 1);

            index_buffer.push_back(i0);
            index_buffer.push_back(i1);
            index_buffer.push_back(i2);
            index_buffer.push_back(i1);
            index_buffer.push_back(i3);
            index_buffer.push_back(i2);

            mesh.m_indices.push_back(i0);
            mesh.m_indices.push_back(i1);
            mesh.m_indices.push_back(i2);
            mesh.m_indices.push_back(i1);
            mesh.m_indices.push_back(i3);
            mesh.m_indices.push_back(i2);
        }
    }
}

void emit_merged_walls(
    WorldMesh& mesh,
    const DungeonMap& map,
    float camera_x,
    float camera_z,
    float anchor_x,
    float anchor_z)
{
    const int width = map.m_width;
    const int height = map.m_height;
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const std::size_t tile_count = static_cast<std::size_t>(width * height);
    std::vector<WallMaterial> materials(tile_count, WallMaterial::None);
    std::vector<float> wall_alpha(tile_count, 1.0F);

    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            const std::size_t idx = tile_index_for(width, col, row);
            const WallMaterial material = wall_material_at(map, col, row);
            materials[idx] = material;
            if (material == WallMaterial::None)
            {
                continue;
            }

            const float x0 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(row) * k_scene_tile_world_size;
            const float x1 = x0 + k_scene_tile_world_size;
            const float z1 = z0 + k_scene_tile_world_size;
            wall_alpha[idx] = calculate_occlusion_alpha(
                x0,
                z0,
                x1,
                z1,
                camera_x,
                camera_z,
                anchor_x,
                anchor_z);
        }
    }

    const auto material_at = [&materials, width, height](int col, int row) {
        if (col < 0 || row < 0 || col >= width || row >= height)
        {
            return WallMaterial::None;
        }
        return materials[tile_index_for(width, col, row)];
    };

    const auto alpha_at = [&wall_alpha, width](int col, int row) {
        return wall_alpha[tile_index_for(width, col, row)];
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

            const int root_bucket = alpha_bucket(wall_alpha[root_idx]);
            int run_width = 1;
            while ((col + run_width) < width)
            {
                const std::size_t idx = tile_index_for(width, col + run_width, row);
                if (top_visited[idx] != 0U || materials[idx] != root_material
                    || alpha_bucket(wall_alpha[idx]) != root_bucket)
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
                    if (top_visited[idx] != 0U || materials[idx] != root_material
                        || alpha_bucket(wall_alpha[idx]) != root_bucket)
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

            float region_alpha = wall_alpha[root_idx];
            for (int r = 0; r < run_height; ++r)
            {
                for (int c = 0; c < run_width; ++c)
                {
                    const std::size_t idx = tile_index_for(width, col + c, row + r);
                    top_visited[idx] = 1U;
                    region_alpha = std::min(region_alpha, wall_alpha[idx]);
                }
            }

            std::vector<uint32_t>& index_buffer =
                (region_alpha < 0.999F) ? mesh.m_transparent_indices : mesh.m_opaque_indices;

            const float x0 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(row) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col + run_width) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row + run_height) * k_scene_tile_world_size;
            emit_wall_top_face(mesh, index_buffer, x0, z0, x1, z1, region_alpha, root_material);
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

            const int bucket = alpha_bucket(alpha_at(col, row));
            int run_start = col;
            float region_alpha = alpha_at(col, row);
            ++col;
            while (col < width
                && material_at(col, row) == material
                && material_at(col, row - 1) == WallMaterial::None
                && alpha_bucket(alpha_at(col, row)) == bucket)
            {
                region_alpha = std::min(region_alpha, alpha_at(col, row));
                ++col;
            }

            std::vector<uint32_t>& index_buffer =
                (region_alpha < 0.999F) ? mesh.m_transparent_indices : mesh.m_opaque_indices;
            const float x0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_north_face(mesh, index_buffer, x0, z, x1, region_alpha, material);
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

            const int bucket = alpha_bucket(alpha_at(col, row));
            int run_start = col;
            float region_alpha = alpha_at(col, row);
            ++col;
            while (col < width
                && material_at(col, row) == material
                && material_at(col, row + 1) == WallMaterial::None
                && alpha_bucket(alpha_at(col, row)) == bucket)
            {
                region_alpha = std::min(region_alpha, alpha_at(col, row));
                ++col;
            }

            std::vector<uint32_t>& index_buffer =
                (region_alpha < 0.999F) ? mesh.m_transparent_indices : mesh.m_opaque_indices;
            const float x0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float x1 = static_cast<float>(col) * k_scene_tile_world_size;
            const float z = static_cast<float>(row + 1) * k_scene_tile_world_size;
            emit_wall_south_face(mesh, index_buffer, x0, z, x1, region_alpha, material);
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

            const int bucket = alpha_bucket(alpha_at(col, row));
            int run_start = row;
            float region_alpha = alpha_at(col, row);
            ++row;
            while (row < height
                && material_at(col, row) == material
                && material_at(col + 1, row) == WallMaterial::None
                && alpha_bucket(alpha_at(col, row)) == bucket)
            {
                region_alpha = std::min(region_alpha, alpha_at(col, row));
                ++row;
            }

            std::vector<uint32_t>& index_buffer =
                (region_alpha < 0.999F) ? mesh.m_transparent_indices : mesh.m_opaque_indices;
            const float x = static_cast<float>(col + 1) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_east_face(mesh, index_buffer, x, z0, z1, region_alpha, material);
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

            const int bucket = alpha_bucket(alpha_at(col, row));
            int run_start = row;
            float region_alpha = alpha_at(col, row);
            ++row;
            while (row < height
                && material_at(col, row) == material
                && material_at(col - 1, row) == WallMaterial::None
                && alpha_bucket(alpha_at(col, row)) == bucket)
            {
                region_alpha = std::min(region_alpha, alpha_at(col, row));
                ++row;
            }

            std::vector<uint32_t>& index_buffer =
                (region_alpha < 0.999F) ? mesh.m_transparent_indices : mesh.m_opaque_indices;
            const float x = static_cast<float>(col) * k_scene_tile_world_size;
            const float z0 = static_cast<float>(run_start) * k_scene_tile_world_size;
            const float z1 = static_cast<float>(row) * k_scene_tile_world_size;
            emit_wall_west_face(mesh, index_buffer, x, z0, z1, region_alpha, material);
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

    emit_merged_walls(mesh, map, camera_x, camera_z, anchor_x, anchor_z);

    for (const TileGeomInput& t : tile_inputs)
    {
        if (t.m_is_wall)
        {
            continue;
        }

        emit_floor_tile(mesh, mesh.m_opaque_indices, t.m_x0, t.m_z0, t.m_x1, t.m_z1);

        const float cx = (t.m_x0 + t.m_x1) * 0.5F;
        const float cz = (t.m_z0 + t.m_z1) * 0.5F;
        constexpr float marker_radius = 10.0F;
        if (t.m_symbol == 'A')
        {
            emit_marker_sphere(mesh, mesh.m_opaque_indices, cx, cz, marker_radius, 0.20F, 0.45F, 0.95F);
        }
        else if (t.m_symbol == 'K')
        {
            emit_marker_sphere(mesh, mesh.m_opaque_indices, cx, cz, marker_radius, 0.95F, 0.85F, 0.20F);
        }
        else if (t.m_symbol == 'S')
        {
            emit_marker_sphere(mesh, mesh.m_opaque_indices, cx, cz, marker_radius, 0.92F, 0.92F, 0.92F);
        }
    }

    return mesh;
}

} // namespace mordor
