#include "pch.h"
#include "Engine/Core/Application.h"

#include <glad/glad.h>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
    }

    void Application::Run() {
        while (!m_Window->ShouldClose()) {
            glViewport(0, 0, (int)m_Window->GetWidth(), (int)m_Window->GetHeight());
            glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            m_Window->OnUpdate();
        }
    }

} // namespace Engine