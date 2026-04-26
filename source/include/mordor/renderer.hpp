#pragma once

#include "mordor/input.hpp"

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace mordor {

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

struct CameraState
{
    float m_x{0.0F};
    float m_y{0.0F};
    float m_zoom{1.0F};
    float m_rotation_radians{0.0F};
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
    CameraState camera_state() const;

    void begin_frame();
    void draw_debug_map(const std::vector<DebugTile>& tiles);
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
};

} // namespace mordor
