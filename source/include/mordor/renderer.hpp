#pragma once

#include "mordor/input.hpp"
#include "mordor/world_mesh.hpp"

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace mordor {
class Scene;
struct DungeonMap;

struct RendererConfig
{
    int m_width{1280};
    int m_height{720};
    const char* m_title{"Mordor"};
};

struct DebugTile
{
    float m_x{0.0F};
    float m_y{0.0F};
    float m_width{10.0F};
    float m_height{10.0F};
    float m_r{0.25F};
    float m_g{0.36F};
    float m_b{0.28F};
};

struct ScreenOverlayRect
{
    int m_x{0};
    int m_y{0};
    int m_width{10};
    int m_height{10};
    float m_r{0.25F};
    float m_g{0.36F};
    float m_b{0.28F};
};

struct FramebufferSize
{
    int m_width{0};
    int m_height{0};
};

struct CameraState
{
    float m_x{0.0F};
    float m_y{0.0F};
    float m_zoom{1.0F};
    float m_rotation_radians{0.0F};
    float m_pitch_radians{0.5236F}; // ~30 degrees (tunable in P5-02)
    float m_distance{1200.0F};      // orbit distance (tunable in P5-02)
};

/// Render submission metrics for performance analysis (P5-05).
struct RenderMetrics
{
    int m_total_vertices{0};      // Total vertices in world mesh
    int m_total_indices{0};       // Total indices (triangles * 3)
    int m_visible_vertices{0};    // Vertices in frustum-visible geometry
    int m_visible_indices{0};     // Indices submitted for rendering
    int m_culled_indices{0};      // Indices culled (not in frustum)
    int m_frustum_culled_nodes{0}; // Scene nodes rejected by frustum test
    int m_frustum_visible_nodes{0}; // Scene nodes inside frustum
};

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(const RendererConfig& config);
    void shutdown();

    bool should_close() const;
    void update_camera_controls(double dt_seconds);
    bool is_input_action_active(InputAction action) const;
    CameraState camera_state() const;
    FramebufferSize framebuffer_size() const;
    void mouse_position(int& out_x, int& out_y) const;  // Current cursor position in screen space
    RenderMetrics render_metrics() const;  // Last frame's render submission metrics (P5-05)

    void begin_frame();
    void draw_debug_map(const std::vector<DebugTile>& tiles);
    void draw_screen_overlay(const std::vector<ScreenOverlayRect>& rects);
    void load_world_mesh(const WorldMesh& mesh);
    void draw_world(const Scene& scene, const DungeonMap& map);  // Updated for P5-05: frustum culling needs scene queries
    void end_frame();

private:
    GLFWwindow* m_window{nullptr};
    bool m_glfw_initialized{false};
    int m_window_width{0};
    int m_window_height{0};

    InputBindings m_input_bindings{};
    CameraState m_camera{};
    float m_pan_speed{350.0F};
    float m_zoom_speed{1.0F};
    float m_rotation_speed{1.5F};
    float m_pitch_speed{1.0F};          // radians per second when adjusting pitch

    // World geometry GPU resources (0 = unloaded).
    uint32_t m_world_shader{0U};
    int      m_mvp_uniform{-1};
    uint32_t m_world_vao{0U};
    uint32_t m_world_vbo{0U};
    uint32_t m_world_ibo_opaque{0U};
    uint32_t m_world_ibo_transparent{0U};
    int      m_world_opaque_index_count{0};
    int      m_world_transparent_index_count{0};

    // Render metrics and frustum culling (P5-05).
    RenderMetrics m_render_metrics{};
    Bounds3 m_frustum_bounds{};  // Camera frustum AABB for culling queries
};

} // namespace mordor
