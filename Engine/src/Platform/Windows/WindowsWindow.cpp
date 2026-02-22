#include "pch.h"
#include "WindowsWindow.h"
#include "Engine/Core/Input.h"
// Keep GLFW from including legacy GL headers
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Engine {

    static bool s_GLFWInitialized = false;

    static void GLFWErrorCallback(int error, const char* description) {
        std::cerr << "[GLFW] Error (" << error << "): " << description << "\n";
    }

    std::unique_ptr<Window> Window::Create(const WindowProps& props) {
        return std::make_unique<WindowsWindow>(props);
    }

    WindowsWindow::WindowsWindow(const WindowProps& props) {
        Init(props);
    }

    WindowsWindow::~WindowsWindow() {
        Shutdown();
    }

    void WindowsWindow::Init(const WindowProps& props) {
        m_Title = props.Title;
        m_Width = props.Width;
        m_Height = props.Height;

        if (!s_GLFWInitialized) {
            glfwSetErrorCallback(GLFWErrorCallback);
            if (!glfwInit()) {
                throw std::runtime_error("Failed to initialize GLFW");
            }
            s_GLFWInitialized = true;
        }

        // Pick 3.3 Core for broad compatibility; change to 4.6 if you want.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        m_Window = glfwCreateWindow((int)m_Width, (int)m_Height, m_Title.c_str(), nullptr, nullptr);
        if (!m_Window) {
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1); // vsync

        Engine::SetNativeGLFWWindow(m_Window);
        

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize glad");
        }

        std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
        std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
        std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << "\n";
    }

    void WindowsWindow::Shutdown() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }

        // Optional: only terminate when your last window is destroyed.
        // For now, single-window app => ok.
        if (s_GLFWInitialized) {
            glfwTerminate();
            s_GLFWInitialized = false;
        }
    }

    void WindowsWindow::OnUpdate() {
        glfwPollEvents();
        glfwSwapBuffers(m_Window);
    }

    bool WindowsWindow::ShouldClose() const {
        return glfwWindowShouldClose(m_Window) != 0;
    }

} // namespace Engine