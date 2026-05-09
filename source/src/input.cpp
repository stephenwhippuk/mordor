#include "mordor/input.hpp"

#if MORDOR_HAS_GLFW
#include <GLFW/glfw3.h>
#endif

namespace mordor {

InputBindings::InputBindings()
{
#if MORDOR_HAS_GLFW
    set_bindings(InputAction::PanLeft, {GLFW_KEY_A, GLFW_KEY_LEFT});
    set_bindings(InputAction::PanRight, {GLFW_KEY_D, GLFW_KEY_RIGHT});
    set_bindings(InputAction::PanUp, {GLFW_KEY_W, GLFW_KEY_UP});
    set_bindings(InputAction::PanDown, {GLFW_KEY_S, GLFW_KEY_DOWN});
    set_bindings(InputAction::RotateLeft, {GLFW_KEY_Q});
    set_bindings(InputAction::RotateRight, {GLFW_KEY_E});
    set_bindings(InputAction::ZoomIn, {GLFW_KEY_R});
    set_bindings(InputAction::ZoomOut, {GLFW_KEY_F});
    set_bindings(InputAction::PitchUp, {GLFW_KEY_T});
    set_bindings(InputAction::PitchDown, {GLFW_KEY_G});
#else
    set_bindings(InputAction::PanLeft, {});
    set_bindings(InputAction::PanRight, {});
    set_bindings(InputAction::PanUp, {});
    set_bindings(InputAction::PanDown, {});
    set_bindings(InputAction::RotateLeft, {});
    set_bindings(InputAction::RotateRight, {});
    set_bindings(InputAction::ZoomIn, {});
    set_bindings(InputAction::ZoomOut, {});
    set_bindings(InputAction::PitchUp, {});
    set_bindings(InputAction::PitchDown, {});
#endif
}

void InputBindings::set_bindings(InputAction action, std::initializer_list<int> keys)
{
    const auto index = static_cast<std::size_t>(action);
    assert(index < static_cast<std::size_t>(InputAction::Count));
    if (index >= static_cast<std::size_t>(InputAction::Count))
    {
        return;
    }
    auto& action_keys = m_bindings[index];
    action_keys.assign(keys.begin(), keys.end());
}

} // namespace mordor
