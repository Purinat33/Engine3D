#include "pch.h"
#include "WindowsWindow.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Events/MouseEvent.h"

namespace Engine {

    static bool s_GLFWInitialized = false;

    static void GLFWErrorCallback(int error, const char* description) {
        std::cerr << "[GLFW] Error (" << error << "): " << description << "\n";
    }

    std::unique_ptr<Window> Window::Create(const WindowProps& props) {
        return std::make_unique<WindowsWindow>(props);
    }

    WindowsWindow::WindowsWindow(const WindowProps& props) { Init(props); }
    WindowsWindow::~WindowsWindow() { Shutdown(); }

    void WindowsWindow::Init(const WindowProps& props) {
        m_Title = props.Title;
        m_Width = props.Width;
        m_Height = props.Height;

        if (!s_GLFWInitialized) {
            glfwSetErrorCallback(GLFWErrorCallback);
            if (!glfwInit())
                throw std::runtime_error("Failed to initialize GLFW");
            s_GLFWInitialized = true;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

        m_Window = glfwCreateWindow((int)m_Width, (int)m_Height, m_Title.c_str(), nullptr, nullptr);
        if (!m_Window)
            throw std::runtime_error("Failed to create GLFW window");

        glfwMakeContextCurrent(m_Window);
        glfwSwapInterval(1);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            throw std::runtime_error("Failed to initialize glad");

        glfwSetWindowUserPointer(m_Window, this);

        // ---- Window events ----
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            ww->m_Width = (uint32_t)width;
            ww->m_Height = (uint32_t)height;

            WindowResizeEvent e((uint32_t)width, (uint32_t)height);
            if (ww->m_EventCallback) ww->m_EventCallback(e);
            });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            WindowCloseEvent e;
            if (ww->m_EventCallback) ww->m_EventCallback(e);
            });

        // ---- Keyboard events ----
        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            if (!ww->m_EventCallback) return;

            switch (action) {
            case GLFW_PRESS: {
                KeyPressedEvent e(key, 0);
                ww->m_EventCallback(e);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent e(key);
                ww->m_EventCallback(e);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent e(key, 1);
                ww->m_EventCallback(e);
                break;
            }
            }
            });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int codepoint) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            if (!ww->m_EventCallback) return;
            KeyTypedEvent e((int)codepoint);
            ww->m_EventCallback(e);
            });

        // ---- Mouse events ----
        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double x, double y) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            if (!ww->m_EventCallback) return;
            MouseMovedEvent e((float)x, (float)y);
            ww->m_EventCallback(e);
            });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xoff, double yoff) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            if (!ww->m_EventCallback) return;
            MouseScrolledEvent e((float)xoff, (float)yoff);
            ww->m_EventCallback(e);
            });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int /*mods*/) {
            auto* ww = (WindowsWindow*)glfwGetWindowUserPointer(window);
            if (!ww->m_EventCallback) return;

            if (action == GLFW_PRESS) {
                MouseButtonPressedEvent e(button);
                ww->m_EventCallback(e);
            }
            else if (action == GLFW_RELEASE) {
                MouseButtonReleasedEvent e(button);
                ww->m_EventCallback(e);
            }
            });

        std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
        std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
        std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << "\n";
    }

    void WindowsWindow::Shutdown() {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
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