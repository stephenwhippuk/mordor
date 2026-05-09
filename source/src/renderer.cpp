#include "mordor/renderer.hpp"

#include "mordor/assert.hpp"
#include "mordor/log.hpp"

#if MORDOR_HAS_GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#if MORDOR_HAS_OPENGL
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

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

bool marker_symbol_supported(char symbol)
{
    return symbol != '\0';
}

void marker_colour(char symbol, float& out_r, float& out_g, float& out_b)
{
    if (symbol == 'A')
    {
        out_r = 0.20F;
        out_g = 0.45F;
        out_b = 0.95F;
        return;
    }

    if (symbol == 'K')
    {
        out_r = 0.95F;
        out_g = 0.85F;
        out_b = 0.20F;
        return;
    }

    if (symbol == 'I')
    {
        out_r = 0.20F;
        out_g = 0.80F;
        out_b = 0.35F;
        return;
    }

    if (symbol == 'N')
    {
        out_r = 0.88F;
        out_g = 0.30F;
        out_b = 0.26F;
        return;
    }

    if (symbol == 'S')
    {
        out_r = 0.92F;
        out_g = 0.92F;
        out_b = 0.92F;
        return;
    }

    out_r = 0.25F;
    out_g = 0.78F;
    out_b = 0.86F;
}

void append_marker_sphere(
    std::vector<WorldVertex>& vertices,
    std::vector<uint32_t>& indices,
    float center_x,
    float center_z,
    char symbol)
{
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    marker_colour(symbol, r, g, b);

    constexpr int stacks = 6;
    constexpr int slices = 10;
    constexpr float radius = 10.0F;
    const float center_y = radius + 2.0F;

    const auto base = static_cast<uint32_t>(vertices.size());
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

            vertices.push_back(WorldVertex{
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

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }
}

void build_runtime_marker_mesh(
    const Scene& scene,
    const std::vector<SceneNodeId>& visible_nodes,
    std::vector<WorldVertex>& out_vertices,
    std::vector<uint32_t>& out_indices)
{
    for (SceneNodeId node_id : visible_nodes)
    {
        const SceneNode* node = find_scene_node(scene, node_id);
        if (node == nullptr || !marker_symbol_supported(node->m_debug_symbol))
        {
            continue;
        }

        append_marker_sphere(out_vertices, out_indices, node->m_world_position.m_x, node->m_world_position.m_y, node->m_debug_symbol);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Matrix math (column-major, matching OpenGL convention)
// ---------------------------------------------------------------------------
#if MORDOR_HAS_OPENGL

namespace {

struct Mat4
{
    float m[16]{};
};

// Standard column-major orthographic projection.
Mat4 mat4_ortho(float l, float r_, float b, float t, float n, float f_)
{
    Mat4 o{};
    o.m[0]  =  2.0F / (r_ - l);
    o.m[5]  =  2.0F / (t  - b);
    o.m[10] = -2.0F / (f_ - n);
    o.m[12] = -(r_ + l) / (r_ - l);
    o.m[13] = -(t  + b) / (t  - b);
    o.m[14] = -(f_ + n) / (f_ - n);
    o.m[15] =  1.0F;
    return o;
}

// Column-major 4x4 multiply: result = a * b.
Mat4 mat4_mul(const Mat4& a, const Mat4& b)
{
    Mat4 c{};
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0F;
            for (int k = 0; k < 4; ++k)
            {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            c.m[col * 4 + row] = sum;
        }
    }
    return c;
}

// Construct a camera view matrix (look-at, column-major).
Mat4 mat4_look_at(
    float ex, float ey, float ez,
    float cx, float cy, float cz,
    float ux, float uy, float uz)
{
    // Forward (eye → center), normalised.
    float fx = cx - ex;
    float fy = cy - ey;
    float fz = cz - ez;
    float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 0.0F) { fx /= flen; fy /= flen; fz /= flen; }

    // Right = forward × world_up, normalised.
    float rx = fy * uz - fz * uy;
    float ry = fz * ux - fx * uz;
    float rz = fx * uy - fy * ux;
    float rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (rlen > 0.0F) { rx /= rlen; ry /= rlen; rz /= rlen; }

    // True up = right × forward.
    const float vux = ry * fz - rz * fy;
    const float vuy = rz * fx - rx * fz;
    const float vuz = rx * fy - ry * fx;

    // Dot helpers for the translation column.
    const float dot_r = rx * ex + ry * ey + rz * ez;
    const float dot_u = vux * ex + vuy * ey + vuz * ez;
    const float dot_f = fx * ex  + fy * ey  + fz * ez;

    // Column-major layout (m[col*4 + row]).
    Mat4 v{};
    v.m[0] = rx;  v.m[1] = vux; v.m[2]  = -fx; v.m[3]  = 0.0F;
    v.m[4] = ry;  v.m[5] = vuy; v.m[6]  = -fy; v.m[7]  = 0.0F;
    v.m[8] = rz;  v.m[9] = vuz; v.m[10] = -fz; v.m[11] = 0.0F;
    v.m[12] = -dot_r; v.m[13] = -dot_u; v.m[14] = dot_f; v.m[15] = 1.0F;
    return v;
}

// ---------------------------------------------------------------------------
// Shader helpers
// ---------------------------------------------------------------------------

GLuint compile_shader_stage(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log_buf[512];
        glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log_buf)), nullptr, log_buf);
        MORDOR_LOG_ERROR("Shader compile error: {}", log_buf);
        glDeleteShader(shader);
        return 0U;
    }
    return shader;
}

GLuint link_shader_program(GLuint vert, GLuint frag)
{
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE)
    {
        char log_buf[512];
        glGetProgramInfoLog(prog, static_cast<GLsizei>(sizeof(log_buf)), nullptr, log_buf);
        MORDOR_LOG_ERROR("Shader link error: {}", log_buf);
        glDeleteProgram(prog);
        return 0U;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// World geometry shader sources
// ---------------------------------------------------------------------------

static constexpr const char* k_world_vert_src = R"glsl(
#version 410 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_col;
layout(location = 2) in float a_alpha;
uniform mat4 u_mvp;
out vec3 v_col;
out float v_alpha;
out vec3 v_world;
void main() {
    gl_Position = u_mvp * vec4(a_pos, 1.0);
    v_col = a_col;
    v_alpha = a_alpha;
    v_world = a_pos;
}
)glsl";

static constexpr const char* k_world_frag_src = R"glsl(
#version 410 core
in vec3 v_col;
in float v_alpha;
in vec3 v_world;
uniform int u_occlusion_pass;
uniform vec2 u_camera_world;
uniform vec2 u_target_world;
out vec4 frag;

float compute_occlusion_alpha(vec2 world_xz)
{
    vec2 seg = u_target_world - u_camera_world;
    float seg_len_sq = dot(seg, seg);
    if (seg_len_sq <= 1e-4)
    {
        return 1.0;
    }

    vec2 to_frag = world_xz - u_camera_world;
    float projected = dot(to_frag, seg) / seg_len_sq;
    if (projected <= 0.0 || projected >= 1.0)
    {
        return 1.0;
    }

    vec2 closest = u_camera_world + (projected * seg);
    float lateral = length(world_xz - closest);
    float corridor_half_width = 36.0; // 0.75 tile
    if (lateral >= corridor_half_width)
    {
        return 1.0;
    }

    float dist_to_target = length(world_xz - u_target_world);
    float radius = 192.0; // 4 tiles
    if (dist_to_target > radius)
    {
        return 1.0;
    }

    float corridor_factor = 1.0 - clamp(lateral / corridor_half_width, 0.0, 1.0);
    float radius_factor = 1.0 - clamp(dist_to_target / radius, 0.0, 1.0);
    float fade_strength = clamp(corridor_factor * radius_factor, 0.0, 1.0);
    return 1.0 - (0.7 * fade_strength);
}

void main() {
    if (u_occlusion_pass == 0)
    {
        frag = vec4(v_col, v_alpha);
        return;
    }

    float dynamic_alpha = compute_occlusion_alpha(v_world.xz);
    if (u_occlusion_pass == 1)
    {
        if (dynamic_alpha < 0.999)
        {
            discard;
        }
        frag = vec4(v_col, 1.0);
        return;
    }

    if (dynamic_alpha >= 0.999)
    {
        discard;
    }
    frag = vec4(v_col, dynamic_alpha);
}
)glsl";

} // namespace (matrix/shader helpers)

#endif // MORDOR_HAS_OPENGL

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
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

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

    // Compile world geometry shaders now that the context is active.
    const GLuint vert = compile_shader_stage(GL_VERTEX_SHADER,   k_world_vert_src);
    const GLuint frag = compile_shader_stage(GL_FRAGMENT_SHADER, k_world_frag_src);
    if (vert == 0U || frag == 0U)
    {
        MORDOR_LOG_ERROR("World geometry shader compilation failed");
        if (vert != 0U) { glDeleteShader(vert); }
        if (frag != 0U) { glDeleteShader(frag); }
        shutdown();
        return false;
    }
    m_world_shader = static_cast<uint32_t>(link_shader_program(vert, frag));
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (m_world_shader == 0U)
    {
        MORDOR_LOG_ERROR("World geometry shader linking failed");
        shutdown();
        return false;
    }
    m_mvp_uniform = glGetUniformLocation(static_cast<GLuint>(m_world_shader), "u_mvp");
    m_occlusion_pass_uniform = glGetUniformLocation(static_cast<GLuint>(m_world_shader), "u_occlusion_pass");
    m_camera_world_uniform = glGetUniformLocation(static_cast<GLuint>(m_world_shader), "u_camera_world");
    m_target_world_uniform = glGetUniformLocation(static_cast<GLuint>(m_world_shader), "u_target_world");
    MORDOR_LOG_INFO("World geometry shaders compiled and linked");
    return true;
#endif
}

void Renderer::shutdown()
{
#if MORDOR_HAS_OPENGL
    // Delete GL objects while the context is still active.
    if (m_world_vao != 0U)
    {
        GLuint vao_gl = static_cast<GLuint>(m_world_vao);
        GLuint vbo_gl = static_cast<GLuint>(m_world_vbo);
        GLuint ibo_opaque_gl = static_cast<GLuint>(m_world_ibo_opaque);
        GLuint ibo_transparent_gl = static_cast<GLuint>(m_world_ibo_transparent);
        glDeleteVertexArrays(1, &vao_gl);
        glDeleteBuffers(1, &vbo_gl);
        if (ibo_opaque_gl != 0U)
        {
            glDeleteBuffers(1, &ibo_opaque_gl);
        }
        if (ibo_transparent_gl != 0U)
        {
            glDeleteBuffers(1, &ibo_transparent_gl);
        }
        m_world_vao = 0U;
        m_world_vbo = 0U;
        m_world_ibo_opaque = 0U;
        m_world_ibo_transparent = 0U;
        m_world_opaque_index_count = 0;
        m_world_transparent_index_count = 0;
    }
    if (m_world_shader != 0U)
    {
        glDeleteProgram(static_cast<GLuint>(m_world_shader));
        m_world_shader = 0U;
    }
#endif
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

    if (is_action_active(InputAction::PitchUp))
    {
        m_camera.m_pitch_radians += m_pitch_speed * dt;
    }
    if (is_action_active(InputAction::PitchDown))
    {
        m_camera.m_pitch_radians -= m_pitch_speed * dt;
    }
    // Clamp pitch: ~5.7° (0.1 rad) min, ~85° (1.48 rad) max to maintain playability
    m_camera.m_pitch_radians = std::clamp(m_camera.m_pitch_radians, 0.1F, 1.48F);
#else
    (void)dt_seconds;
#endif
}

bool Renderer::is_input_action_active(InputAction action) const
{
#if MORDOR_HAS_GLFW
    if (m_window == nullptr)
    {
        return false;
    }

    return m_input_bindings.is_action_active(action, [this](int key) {
        return key_down(m_window, key);
    });
#else
    (void)action;
    return false;
#endif
}

void Renderer::set_occlusion_target_world(float world_x, float world_y)
{
    m_occlusion_target_x = world_x;
    m_occlusion_target_y = world_y;
}

CameraState Renderer::camera_state() const
{
    return m_camera;
}

FramebufferSize Renderer::framebuffer_size() const
{
    return FramebufferSize{.m_width = m_window_width, .m_height = m_window_height};
}

void Renderer::mouse_position(int& out_x, int& out_y) const
{
#if MORDOR_HAS_GLFW
    if (m_window != nullptr)
    {
        double mouse_x = 0.0;
        double mouse_y = 0.0;
        glfwGetCursorPos(m_window, &mouse_x, &mouse_y);
        out_x = static_cast<int>(mouse_x);
        out_y = static_cast<int>(mouse_y);
        return;
    }
#endif
    out_x = 0;
    out_y = 0;
}

RenderMetrics Renderer::render_metrics() const
{
    return m_render_metrics;
}

void Renderer::begin_frame()
{
#if MORDOR_HAS_OPENGL
    MORDOR_ASSERT_MSG(m_window != nullptr, "Renderer not initialised");

    glfwGetFramebufferSize(m_window, &m_window_width, &m_window_height);
    glViewport(0, 0, m_window_width, m_window_height);

    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.08F, 0.09F, 0.12F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

void Renderer::draw_screen_overlay(const std::vector<ScreenOverlayRect>& rects)
{
#if MORDOR_HAS_OPENGL
    MORDOR_ASSERT_MSG(m_window != nullptr, "Renderer not initialised");

    glEnable(GL_SCISSOR_TEST);

    for (const ScreenOverlayRect& rect : rects)
    {
        if (rect.m_width <= 0 || rect.m_height <= 0)
        {
            continue;
        }

        const int scissor_y = m_window_height - rect.m_y - rect.m_height;
        if (scissor_y < 0 || scissor_y >= m_window_height)
        {
            continue;
        }

        glScissor(rect.m_x, scissor_y, rect.m_width, rect.m_height);
        glClearColor(rect.m_r, rect.m_g, rect.m_b, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glDisable(GL_SCISSOR_TEST);
#else
    (void)rects;
#endif
}

void Renderer::load_world_mesh(const WorldMesh& mesh)
{
#if MORDOR_HAS_OPENGL
    if (m_window == nullptr || mesh.m_vertices.empty()
        || (mesh.m_opaque_indices.empty() && mesh.m_transparent_indices.empty()))
    {
        return;
    }

    // Release any previously loaded mesh.
    if (m_world_vao != 0U)
    {
        GLuint vao_gl = static_cast<GLuint>(m_world_vao);
        GLuint vbo_gl = static_cast<GLuint>(m_world_vbo);
        GLuint ibo_opaque_gl = static_cast<GLuint>(m_world_ibo_opaque);
        GLuint ibo_transparent_gl = static_cast<GLuint>(m_world_ibo_transparent);
        glDeleteVertexArrays(1, &vao_gl);
        glDeleteBuffers(1, &vbo_gl);
        if (ibo_opaque_gl != 0U)
        {
            glDeleteBuffers(1, &ibo_opaque_gl);
        }
        if (ibo_transparent_gl != 0U)
        {
            glDeleteBuffers(1, &ibo_transparent_gl);
        }
        m_world_vao = 0U;
        m_world_vbo = 0U;
        m_world_ibo_opaque = 0U;
        m_world_ibo_transparent = 0U;
        m_world_opaque_index_count = 0;
        m_world_transparent_index_count = 0;
    }

    GLuint vao = 0U;
    GLuint vbo = 0U;
    GLuint ibo_opaque = 0U;
    GLuint ibo_transparent = 0U;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    if (!mesh.m_opaque_indices.empty())
    {
        glGenBuffers(1, &ibo_opaque);
    }
    if (!mesh.m_transparent_indices.empty())
    {
        glGenBuffers(1, &ibo_transparent);
    }

    glBindVertexArray(vao);

    // Upload vertex data.
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mesh.m_vertices.size() * sizeof(WorldVertex)),
        mesh.m_vertices.data(),
        GL_STATIC_DRAW);

    // Attribute 0: position (vec3, offset 0).
    const GLsizei stride = static_cast<GLsizei>(sizeof(WorldVertex));
    glVertexAttribPointer(
        0U, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(WorldVertex, m_x)));
    glEnableVertexAttribArray(0U);

    // Attribute 1: colour (vec3, offset 12).
    glVertexAttribPointer(
        1U, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(WorldVertex, m_r)));
    glEnableVertexAttribArray(1U);

    // Attribute 2: alpha (float, offset 24).
    glVertexAttribPointer(
        2U, 1, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<const void*>(offsetof(WorldVertex, m_alpha)));
    glEnableVertexAttribArray(2U);

    if (ibo_opaque != 0U)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_opaque);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.m_opaque_indices.size() * sizeof(uint32_t)),
            mesh.m_opaque_indices.data(),
            GL_STATIC_DRAW);
    }

    if (ibo_transparent != 0U)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_transparent);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.m_transparent_indices.size() * sizeof(uint32_t)),
            mesh.m_transparent_indices.data(),
            GL_STATIC_DRAW);
    }

    glBindVertexArray(0U);

    m_world_vao         = static_cast<uint32_t>(vao);
    m_world_vbo         = static_cast<uint32_t>(vbo);
    m_world_ibo_opaque = static_cast<uint32_t>(ibo_opaque);
    m_world_opaque_index_count = static_cast<int>(mesh.m_opaque_indices.size());

    m_world_ibo_transparent = static_cast<uint32_t>(ibo_transparent);
    m_world_transparent_index_count = static_cast<int>(mesh.m_transparent_indices.size());

    MORDOR_LOG_INFO(
        "World mesh loaded: vertices={} opaque_indices={} transparent_indices={}",
        mesh.m_vertices.size(),
        mesh.m_opaque_indices.size(),
        mesh.m_transparent_indices.size());
#else
    (void)mesh;
#endif
}

void Renderer::draw_world(const Scene& scene, const DungeonMap& map)
{
#if MORDOR_HAS_OPENGL
    (void)map;
    if (m_window == nullptr || m_world_shader == 0U
        || m_world_vao == 0U
        || (m_world_opaque_index_count <= 0 && m_world_transparent_index_count <= 0))
    {
        return;
    }

    // Tunable camera orbit parameters (configurable in P5-02).
    const float pitch = m_camera.m_pitch_radians;
    const float dist  = m_camera.m_distance;

    const float yaw       = m_camera.m_rotation_radians;
    const float cos_pitch = std::cos(pitch);
    const float sin_pitch = std::sin(pitch);

    const float eye_x = m_camera.m_x + std::sin(yaw) * cos_pitch * dist;
    const float eye_y = sin_pitch * dist;
    const float eye_z = m_camera.m_y + std::cos(yaw) * cos_pitch * dist;

    const Mat4 view = mat4_look_at(
        eye_x, eye_y, eye_z,
        m_camera.m_x, 0.0F, m_camera.m_y,
        0.0F, 1.0F, 0.0F);

    const float half_w = static_cast<float>(m_window_width)  * 0.5F / m_camera.m_zoom;
    const float half_h = static_cast<float>(m_window_height) * 0.5F / m_camera.m_zoom;
    const Mat4 proj    = mat4_ortho(-half_w, half_w, -half_h, half_h, -2000.0F, 2000.0F);
    const Mat4 mvp     = mat4_mul(proj, view);

    // P5-05: Calculate frustum AABB for culling (ortho projection bounds + depth range).
    // Frustum AABB in world space: camera XZ position +/- half viewport, depth from eye.
    m_frustum_bounds = Bounds3{
        .m_min = Float3{
            .m_x = m_camera.m_x - half_w,
            .m_y = eye_y - 1000.0F,  // Depth range below eye
            .m_z = m_camera.m_y - half_h,
        },
        .m_max = Float3{
            .m_x = m_camera.m_x + half_w,
            .m_y = eye_y + 1000.0F,  // Depth range above eye
            .m_z = m_camera.m_y + half_h,
        },
    };

    // Query scene for geometry within frustum (P5-05).
    const std::vector<SceneNodeId> visible_nodes = query_scene_bounds(scene, m_frustum_bounds);

    // P5-05: Calculate render metrics.
    m_render_metrics.m_total_indices = m_world_opaque_index_count + m_world_transparent_index_count;
    m_render_metrics.m_total_vertices = static_cast<int>(m_render_metrics.m_total_indices / 3);  // Approximate: 1 triangle = 3 indices
    m_render_metrics.m_frustum_visible_nodes = static_cast<int>(visible_nodes.size());
    m_render_metrics.m_frustum_culled_nodes = static_cast<int>(scene.m_nodes.size() - visible_nodes.size());
    // For now, submit all geometry (actual culling is future optimization).
    m_render_metrics.m_visible_indices = m_render_metrics.m_total_indices;
    m_render_metrics.m_visible_vertices = m_render_metrics.m_total_vertices;
    m_render_metrics.m_culled_indices = 0;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(static_cast<GLuint>(m_world_shader));
    glUniformMatrix4fv(m_mvp_uniform, 1, GL_FALSE, mvp.m);
    glUniform2f(m_camera_world_uniform, eye_x, eye_z);
    glUniform2f(m_target_world_uniform, m_occlusion_target_x, m_occlusion_target_y);

    glBindVertexArray(static_cast<GLuint>(m_world_vao));

    if (m_world_opaque_index_count > 0 && m_world_ibo_opaque != 0U)
    {
        glUniform1i(m_occlusion_pass_uniform, 0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(m_world_ibo_opaque));
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(m_world_opaque_index_count),
            GL_UNSIGNED_INT,
            nullptr);
    }

    if (m_world_transparent_index_count > 0 && m_world_ibo_transparent != 0U)
    {
        glUniform1i(m_occlusion_pass_uniform, 1);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(m_world_ibo_transparent));
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(m_world_transparent_index_count),
            GL_UNSIGNED_INT,
            nullptr);

        glUniform1i(m_occlusion_pass_uniform, 2);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(m_world_transparent_index_count),
            GL_UNSIGNED_INT,
            nullptr);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    std::vector<WorldVertex> marker_vertices{};
    std::vector<uint32_t> marker_indices{};
    build_runtime_marker_mesh(scene, visible_nodes, marker_vertices, marker_indices);
    if (!marker_vertices.empty() && !marker_indices.empty())
    {
        GLuint marker_vao = 0U;
        GLuint marker_vbo = 0U;
        GLuint marker_ibo = 0U;
        glGenVertexArrays(1, &marker_vao);
        glGenBuffers(1, &marker_vbo);
        glGenBuffers(1, &marker_ibo);

        glBindVertexArray(marker_vao);
        glBindBuffer(GL_ARRAY_BUFFER, marker_vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(marker_vertices.size() * sizeof(WorldVertex)),
            marker_vertices.data(),
            GL_DYNAMIC_DRAW);

        const GLsizei stride = static_cast<GLsizei>(sizeof(WorldVertex));
        glVertexAttribPointer(
            0U, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(offsetof(WorldVertex, m_x)));
        glEnableVertexAttribArray(0U);
        glVertexAttribPointer(
            1U, 3, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(offsetof(WorldVertex, m_r)));
        glEnableVertexAttribArray(1U);
        glVertexAttribPointer(
            2U, 1, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(offsetof(WorldVertex, m_alpha)));
        glEnableVertexAttribArray(2U);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, marker_ibo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(marker_indices.size() * sizeof(uint32_t)),
            marker_indices.data(),
            GL_DYNAMIC_DRAW);

        glUniform1i(m_occlusion_pass_uniform, 0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(marker_indices.size()),
            GL_UNSIGNED_INT,
            nullptr);

        glBindVertexArray(0U);
        glDeleteVertexArrays(1, &marker_vao);
        glDeleteBuffers(1, &marker_vbo);
        glDeleteBuffers(1, &marker_ibo);
    }

    glBindVertexArray(0U);

    glUseProgram(0U);
    glDisable(GL_DEPTH_TEST);
#else
    (void)scene;
    (void)map;
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
