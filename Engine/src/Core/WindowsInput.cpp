#include "pch.h"
#include "Engine/Core/Input.h"

#include <GLFW/glfw3.h>

namespace Engine {

    static GLFWwindow* s_Window = nullptr;

    void SetNativeGLFWWindow(GLFWwindow* w) {
        s_Window = w;
    }

    static GLFWwindow* GetGLFWWindow() {
        if (!s_Window)
            s_Window = glfwGetCurrentContext();
        return s_Window;
    }

    bool Input::IsKeyDown(KeyCode key) {
        GLFWwindow* w = GetGLFWWindow();
        if (!w) return false;
        int state = glfwGetKey(w, (int)key);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool Input::IsMouseButtonDown(int button) {
        GLFWwindow* w = GetGLFWWindow();
        if (!w) return false;
        return glfwGetMouseButton(w, button) == GLFW_PRESS;
    }

    void Input::GetMousePosition(float& x, float& y) {
        GLFWwindow* w = GetGLFWWindow();
        if (!w) { x = y = 0.0f; return; }
        double dx, dy;
        glfwGetCursorPos(w, &dx, &dy);
        x = (float)dx;
        y = (float)dy;
    }

} // namespace Engine