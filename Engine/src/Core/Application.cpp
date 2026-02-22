#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
    }

    void Application::Run() {
        // Triangle vertices: position (x,y,z) + color (r,g,b)
        float vertices[] = {
            //  x     y     z      r     g     b
            -0.5f, -0.5f, 0.0f,  1.0f, 0.2f, 0.2f,
             0.5f, -0.5f, 0.0f,  0.2f, 1.0f, 0.2f,
             0.0f,  0.5f, 0.0f,  0.2f, 0.2f, 1.0f
        };
        uint32_t indices[] = { 0, 1, 2 };

        auto vao = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(vertices, sizeof(vertices));
        vb->SetLayout({
            { ShaderDataType::Float3 }, // position
            { ShaderDataType::Float3 }  // color
            });
        vao->AddVertexBuffer(vb);

        auto ib = std::make_shared<IndexBuffer>(indices, 3);
        vao->SetIndexBuffer(ib);

        const std::string vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            vColor = aColor;
            gl_Position = vec4(aPos, 1.0);
        }
    )";

        const std::string fs = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )";

        Shader shader(vs, fs);

        while (!m_Window->ShouldClose()) {
            glViewport(0, 0, (int)m_Window->GetWidth(), (int)m_Window->GetHeight());
            glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            shader.Bind();
            vao->Bind();
            glDrawElements(GL_TRIANGLES, (int)vao->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);

            m_Window->OnUpdate();
        }
    }

} // namespace Engine