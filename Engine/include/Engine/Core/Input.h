#pragma once
#include <cstdint>

struct GLFWwindow; // <-- forward declare (no GLFW include needed)

namespace Engine {

    enum class KeyCode : int {
        W = 87, A = 65, S = 83, D = 68,
        Q = 81, E = 69,
        LeftShift = 340,
        Escape = 256
    };

    class Input {
    public:
        static bool IsKeyDown(KeyCode key);
        static bool IsMouseButtonDown(int button);
        static void GetMousePosition(float& x, float& y);
    };

    // <-- ADD THIS
    void SetNativeGLFWWindow(GLFWwindow* w);

} // namespace Engine