#pragma once

#include "mordor/map.hpp"
#include "mordor/scene.hpp"

#include <cstdint>
#include <vector>

namespace mordor {

/// Height of wall geometry above the floor plane, in world units.
constexpr float k_wall_height = 48.0F;

/// Single vertex in the 3D world geometry buffer: position + colour + alpha.
/// Layout: (x, y, z, r, g, b, a) — 7 floats, 28 bytes, no padding.
struct WorldVertex
{
    float m_x{0.0F};
    float m_y{0.0F};
    float m_z{0.0F};
    float m_r{1.0F};
    float m_g{1.0F};
    float m_b{1.0F};
    float m_alpha{1.0F};  // Opacity: 1.0 = opaque, 0.5 = faded (occluded), 0.0 = invisible
};

/// CPU-side world geometry ready for GPU upload.
struct WorldMesh
{
    std::vector<WorldVertex> m_vertices{};
    std::vector<uint32_t>   m_indices{};
};

/// Build floor and wall geometry from scene node world bounds and map tile data.
/// Floor tiles emit a flat quad at Y = 0.
/// Wall tiles emit a box (top face + four side faces) from Y = 0 to Y = k_wall_height.
/// Wall alpha is calculated in an actor-centric camera-to-anchor corridor for interaction readability.
/// Tiles are emitted in ascending payload-index (row-major) order for deterministic output.
/// camera_x, camera_z: Camera/world observer position in world space.
/// anchor_x, anchor_z: Active actor/party anchor position in world space.
WorldMesh build_world_mesh(
    const Scene& scene,
    const DungeonMap& map,
    float camera_x,
    float camera_z,
    float anchor_x,
    float anchor_z);

} // namespace mordor
