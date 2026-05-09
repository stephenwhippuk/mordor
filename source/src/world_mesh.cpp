#include "mordor/world_mesh.hpp"

#include "mordor/scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace mordor {

namespace {

struct TileGeomInput
{
    float m_x0{0.0F};
    float m_z0{0.0F};
    float m_x1{0.0F};
    float m_z1{0.0F};
    bool  m_is_wall{false};
    int   m_sort_key{0}; // payload_index = row * width + col
};

void emit_quad(
    WorldMesh&  mesh,
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
    // Two CCW triangles sharing diagonal v0-v2.
    mesh.m_indices.push_back(base + 0U);
    mesh.m_indices.push_back(base + 1U);
    mesh.m_indices.push_back(base + 2U);
    mesh.m_indices.push_back(base + 0U);
    mesh.m_indices.push_back(base + 2U);
    mesh.m_indices.push_back(base + 3U);
}

float calculate_occlusion_alpha(float wall_x0, float wall_z0, float wall_x1, float wall_z1, float camera_x, float camera_z)
{
    // Wall center in XZ plane.
    const float wall_cx = (wall_x0 + wall_x1) * 0.5F;
    const float wall_cz = (wall_z0 + wall_z1) * 0.5F;

    // Distance from wall center to camera.
    const float dx = camera_x - wall_cx;
    const float dz = camera_z - wall_cz;
    const float distance = std::sqrt(dx * dx + dz * dz);

    // Fade range: walls closer than 200 units start fading, reach minimum at 100 units.
    // min_alpha = 0.3 (wall still partially visible), max_alpha = 1.0 (fully opaque).
    constexpr float fade_start = 200.0F;  // Distance where fade begins
    constexpr float fade_end   = 100.0F;  // Distance where fade reaches minimum
    constexpr float min_alpha  = 0.3F;    // Minimum opacity (wall not invisible)
    constexpr float max_alpha  = 1.0F;    // Maximum opacity (fully opaque)

    if (distance >= fade_start)
    {
        return max_alpha;  // Far away: fully opaque
    }
    if (distance <= fade_end)
    {
        return min_alpha;  // Very close: barely visible
    }

    // Linear fade between min and max.
    const float t = (distance - fade_end) / (fade_start - fade_end);
    return min_alpha + (max_alpha - min_alpha) * t;
}

void emit_floor_tile(WorldMesh& mesh, float x0, float z0, float x1, float z1)
{
    // Flat quad at Y = 0, CCW winding viewed from above (+Y).
    constexpr float r = 0.18F;
    constexpr float g = 0.43F;
    constexpr float b = 0.24F;
    emit_quad(
        mesh,
        WorldVertex{x0, 0.0F, z0, r, g, b},
        WorldVertex{x1, 0.0F, z0, r, g, b},
        WorldVertex{x1, 0.0F, z1, r, g, b},
        WorldVertex{x0, 0.0F, z1, r, g, b});
}

void emit_wall_tile(WorldMesh& mesh, float x0, float z0, float x1, float z1, float alpha)
{
    const float h = k_wall_height;

    // Top face — slightly lighter stone.
    constexpr float tr = 0.62F;
    constexpr float tg = 0.20F;
    constexpr float tb = 0.20F;
    emit_quad(
        mesh,
        WorldVertex{x0, h, z0, tr, tg, tb, alpha},
        WorldVertex{x1, h, z0, tr, tg, tb, alpha},
        WorldVertex{x1, h, z1, tr, tg, tb, alpha},
        WorldVertex{x0, h, z1, tr, tg, tb, alpha});

    // South face (Z+).
    constexpr float fr = 0.50F;
    constexpr float fg = 0.16F;
    constexpr float fb = 0.16F;
    emit_quad(
        mesh,
        WorldVertex{x0, 0.0F, z1, fr, fg, fb, alpha},
        WorldVertex{x1, 0.0F, z1, fr, fg, fb, alpha},
        WorldVertex{x1, h,    z1, fr, fg, fb, alpha},
        WorldVertex{x0, h,    z1, fr, fg, fb, alpha});

    // North face (Z-).
    emit_quad(
        mesh,
        WorldVertex{x1, 0.0F, z0, fr, fg, fb, alpha},
        WorldVertex{x0, 0.0F, z0, fr, fg, fb, alpha},
        WorldVertex{x0, h,    z0, fr, fg, fb, alpha},
        WorldVertex{x1, h,    z0, fr, fg, fb, alpha});

    // East face (X+).
    constexpr float sr = 0.40F;
    constexpr float sg = 0.13F;
    constexpr float sb = 0.13F;
    emit_quad(
        mesh,
        WorldVertex{x1, 0.0F, z0, sr, sg, sb, alpha},
        WorldVertex{x1, 0.0F, z1, sr, sg, sb, alpha},
        WorldVertex{x1, h,    z1, sr, sg, sb, alpha},
        WorldVertex{x1, h,    z0, sr, sg, sb, alpha});

    // West face (X-).
    emit_quad(
        mesh,
        WorldVertex{x0, 0.0F, z1, sr, sg, sb, alpha},
        WorldVertex{x0, 0.0F, z0, sr, sg, sb, alpha},
        WorldVertex{x0, h,    z0, sr, sg, sb, alpha},
        WorldVertex{x0, h,    z1, sr, sg, sb, alpha});
}

} // namespace

WorldMesh build_world_mesh(const Scene& scene, const DungeonMap& map, float camera_x, float camera_z)
{
    WorldMesh mesh{};

    if (map.m_tiles.empty() || map.m_width <= 0 || map.m_height <= 0)
    {
        return mesh;
    }

    // Collect renderable static geometry nodes that reference a valid tile.
    // Scene XY bounds map to 3D XZ plane; 3D Y is the vertical axis.
    const uint32_t renderable_static_flags =
        scene_node_category_bits(SceneNodeCategory::StaticGeometry)
        | scene_node_category_bits(SceneNodeCategory::Renderable);

    std::vector<TileGeomInput> tile_inputs{};
    tile_inputs.reserve(map.m_tiles.size());

    for (const SceneNode& node : scene.m_nodes)
    {
        if (!scene_node_has_flags(node, renderable_static_flags))
        {
            continue;
        }

        if (node.m_payload_index < 0)
        {
            continue;
        }

        const auto tile_index = static_cast<std::size_t>(node.m_payload_index);
        if (tile_index >= map.m_tiles.size())
        {
            continue;
        }

        const DungeonTile& tile = map.m_tiles[tile_index];

        // Scene uses XY plane (X = col * tile_size, Y = row * tile_size).
        // Remap: scene X -> 3D X, scene Y -> 3D Z.
        tile_inputs.push_back(TileGeomInput{
            .m_x0 = node.m_world_bounds.m_min.m_x,
            .m_z0 = node.m_world_bounds.m_min.m_y,
            .m_x1 = node.m_world_bounds.m_max.m_x,
            .m_z1 = node.m_world_bounds.m_max.m_y,
            .m_is_wall  = tile.m_blocks_movement,
            .m_sort_key = node.m_payload_index,
        });
    }

    // Sort by payload index (row-major) for deterministic draw ordering.
    std::sort(
        tile_inputs.begin(),
        tile_inputs.end(),
        [](const TileGeomInput& a, const TileGeomInput& b) {
            return a.m_sort_key < b.m_sort_key;
        });

    // Reserve upper-bound capacity: wall = 20 verts + 30 indices, floor = 4 + 6.
    mesh.m_vertices.reserve(tile_inputs.size() * 20);
    mesh.m_indices.reserve(tile_inputs.size() * 30);

    for (const TileGeomInput& t : tile_inputs)
    {
        if (t.m_is_wall)
        {
            const float alpha = calculate_occlusion_alpha(t.m_x0, t.m_z0, t.m_x1, t.m_z1, camera_x, camera_z);
            emit_wall_tile(mesh, t.m_x0, t.m_z0, t.m_x1, t.m_z1, alpha);
        }
        else
        {
            emit_floor_tile(mesh, t.m_x0, t.m_z0, t.m_x1, t.m_z1);
        }
    }

    return mesh;
}

} // namespace mordor
