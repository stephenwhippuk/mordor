#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <vector>

namespace mordor {

enum class InputAction : std::size_t
{
    PanLeft = 0,
    PanRight,
    PanUp,
    PanDown,
    RotateLeft,
    RotateRight,
    ZoomIn,
    ZoomOut,
    Count
};

class InputBindings
{
public:
    InputBindings();

    void set_bindings(InputAction action, std::initializer_list<int> keys);

    template <typename IsKeyDownFn>
    bool is_action_active(InputAction action, IsKeyDownFn&& is_key_down) const
    {
        const auto index = static_cast<std::size_t>(action);
        assert(index < static_cast<std::size_t>(InputAction::Count));
        if (index >= static_cast<std::size_t>(InputAction::Count))
        {
            return false;
        }
        const auto& keys = m_bindings[index];
        for (const int key : keys)
        {
            if (is_key_down(key))
            {
                return true;
            }
        }
        return false;
    }

private:
    std::array<std::vector<int>, static_cast<std::size_t>(InputAction::Count)> m_bindings{};
};

} // namespace mordor
