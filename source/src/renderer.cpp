#include "mordor/renderer.hpp"

#include "mordor/assert.hpp"
#include "mordor/log.hpp"

#if MORDOR_HAS_GLFW
#include <GLFW/glfw3.h>
#endif

#if MORDOR_HAS_OPENGL
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#endif

#include <algorithm>
#include <cmath>

namespace mordor {

namespace {

#if MORDOR_HAS_GLFW
void glfw_error_callback(int code, const char* description)
{
    MORDOR_LOG_ERROR("GLFW error {}: {}", code, description ? description : "unknown");
}

bool key_down(GLFWwindow* window, int key)
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}
#endif

} // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::init(const RendererConfig& config)
{
#if !MORDOR_HAS_GLFW || !MORDOR_HAS_OPENGL
    (void)config;
    MORDOR_LOG_WARN("Renderer running in stub mode: GLFW/OpenGL dependencies not available");
    return true;
#else
    MORDOR_ASSERT_MSG(config.m_width > 0 && config.m_height > 0, "invalid window size");

    glfwSetErrorCallback(glfw_error_callback);

    if (glfwInit() == GLFW_FALSE)
    {
        MORDOR_LOG_ERROR("Failed to initialise GLFW");
        return false;
    }
    m_glfw_initialized = true;

    // Decision 0002 baseline: OpenGL 4.1 core profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(config.m_width, config.m_height, config.m_title, nullptr, nullptr);
    if (m_window == nullptr)
    {
        MORDOR_LOG_ERROR("Failed to create OpenGL window");
        glfwTerminate();
        m_glfw_initialized = false;
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    int major = 0;
    int minor = 0;
    major = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MINOR);

    if (major < 4 || (major == 4 && minor < 1))
    {
        MORDOR_LOG_ERROR("OpenGL 4.1+ required, got {}.{}", major, minor);
        shutdown();
        return false;
    }

    m_window_width = config.m_width;
    m_window_height = config.m_height;

    MORDOR_LOG_INFO("Renderer initialised with OpenGL {}.{}", major, minor);
    return true;
#endif
}

void Renderer::shutdown()
{
#if MORDOR_HAS_GLFW
    if (m_window != nullptr)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    if (m_glfw_initialized)
    {
        glfwTerminate();
        m_glfw_initialized = false;
    }
#endif
}

bool Renderer::should_close() const
{
#if MORDOR_HAS_GLFW
    return (m_window == nullptr) || (glfwWindowShouldClose(m_window) == GLFW_TRUE);
#else
    return false;
#endif
}

void Renderer::update_camera_controls(double dt_seconds)
{
#if MORDOR_HAS_GLFW
    if (m_window == nullptr)
    {
        return;
    }

    const float dt = static_cast<float>(dt_seconds);

    float move_x = 0.0F;
    float move_y = 0.0F;

    const auto is_action_active = [this](InputAction action) {
        return m_input_bindings.is_action_active(action, [this](int key) {
            return key_down(m_window, key);
        });
    };

    if (is_action_active(InputAction::PanLeft))
    {
        move_x -= 1.0F;
    }
    if (is_action_active(InputAction::PanRight))
    {
        move_x += 1.0F;
    }
    if (is_action_active(InputAction::PanUp))
    {
        move_y += 1.0F;
    }
    if (is_action_active(InputAction::PanDown))
    {
        move_y -= 1.0F;
    }

    if (move_x != 0.0F || move_y != 0.0F)
    {
        const float mag = std::sqrt((move_x * move_x) + (move_y * move_y));
        move_x /= mag;
        move_y /= mag;

        m_camera.m_x += move_x * m_pan_speed * dt;
        m_camera.m_y += move_y * m_pan_speed * dt;
    }

    if (is_action_active(InputAction::RotateLeft))
    {
        m_camera.m_rotation_radians -= m_rotation_speed * dt;
    }
    if (is_action_active(InputAction::RotateRight))
    {
        m_camera.m_rotation_radians += m_rotation_speed * dt;
    }

    if (is_action_active(InputAction::ZoomIn))
    {
        m_camera.m_zoom += m_zoom_speed * dt;
    }
    if (is_action_active(InputAction::ZoomOut))
    {
        m_camera.m_zoom -= m_zoom_speed * dt;
    }
    m_camera.m_zoom = std::clamp(m_camera.m_zoom, 0.2F, 4.0F);
#else
    (void)dt_seconds;
#endif
}

CameraState Renderer::camera_state() const
{
    return m_camera;
}

void Renderer::begin_frame()
{
#if MORDOR_HAS_OPENGL
    MORDOR_ASSERT_MSG(m_window != nullptr, "Renderer not initialised");

    glfwGetFramebufferSize(m_window, &m_window_width, &m_window_height);
    glViewport(0, 0, m_window_width, m_window_height);

    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.08F, 0.09F, 0.12F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

void Renderer::draw_debug_map(const std::vector<DebugTile>& tiles)
{
#if MORDOR_HAS_OPENGL
    MORDOR_ASSERT_MSG(m_window != nullptr, "Renderer not initialised");

    // Temporary map draw path without shader pipeline: use scissor-constrained clears.
    glEnable(GL_SCISSOR_TEST);

    const float c = std::cos(m_camera.m_rotation_radians);
    const float s = std::sin(m_camera.m_rotation_radians);

    for (const DebugTile& tile : tiles)
    {
        const float world_cx = tile.m_x - m_camera.m_x;
        const float world_cy = tile.m_y - m_camera.m_y;

        const float scaled_cx = world_cx * m_camera.m_zoom;
        const float scaled_cy = world_cy * m_camera.m_zoom;

        const float rot_cx = (scaled_cx * c) - (scaled_cy * s);
        const float rot_cy = (scaled_cx * s) + (scaled_cy * c);

        const float screen_cx = (static_cast<float>(m_window_width) * 0.5F) + rot_cx;
        const float screen_cy = (static_cast<float>(m_window_height) * 0.5F) + rot_cy;

        const float scaled_w = tile.m_width * m_camera.m_zoom;
        const float scaled_h = tile.m_height * m_camera.m_zoom;

        if (scaled_w <= 0.0F || scaled_h <= 0.0F)
        {
            continue;
        }

        const float half_w = scaled_w * 0.5F;
        const float half_h = scaled_h * 0.5F;
        const float abs_c = std::abs(c);
        const float abs_s = std::abs(s);
        const float aabb_half_w = (abs_c * half_w) + (abs_s * half_h);
        const float aabb_half_h = (abs_s * half_w) + (abs_c * half_h);

        const int x = static_cast<int>(std::floor(screen_cx - aabb_half_w));
        const int y = static_cast<int>(std::floor(screen_cy - aabb_half_h));
        const int right = static_cast<int>(std::ceil(screen_cx + aabb_half_w));
        const int top = static_cast<int>(std::ceil(screen_cy + aabb_half_h));
        const int w = right - x;
        const int h = top - y;
        if (w <= 0 || h <= 0)
        {
            continue;
        }

        glScissor(x, y, w, h);
        glClearColor(tile.m_r, tile.m_g, tile.m_b, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glDisable(GL_SCISSOR_TEST);
#else
    (void)tiles;
#endif
}

void Renderer::end_frame()
{
#if MORDOR_HAS_OPENGL
    MORDOR_ASSERT_MSG(m_window != nullptr, "Renderer not initialised");

    glfwSwapBuffers(m_window);
    glfwPollEvents();
#endif
}

} // namespace mordor
