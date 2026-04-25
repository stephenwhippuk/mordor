#pragma once

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
    void begin_frame();
    void draw_debug_map(const std::vector<DebugTile>& tiles);
    void end_frame();

private:
    GLFWwindow* m_window{nullptr};
    bool m_glfw_initialized{false};
    int m_window_width{0};
    int m_window_height{0};
};

} // namespace mordor
