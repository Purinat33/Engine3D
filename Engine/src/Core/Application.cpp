#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        Renderer::Init();
    }

    void Application::Run() {
        // Triangle (pos only for now)
        float vertices[] = {
            // x     y     z
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };
        uint32_t indices[] = { 0, 1, 2 };

        auto vao = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(vertices, sizeof(vertices));
        vb->SetLayout({ { ShaderDataType::Float3 } });
        vao->AddVertexBuffer(vb);

        auto ib = std::make_shared<IndexBuffer>(indices, 3);
        vao->SetIndexBuffer(ib);

        const std::string vs = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 u_ViewProjection;
        uniform mat4 u_Model;

        void main() {
            gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
        }
    )";

        const std::string fs = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(0.2, 0.8, 0.4, 1.0);
        }
    )";

        auto shader = std::make_shared<Shader>(vs, fs);

        PerspectiveCamera camera(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);
        camera.SetPosition({ 0.0f, 0.0f, 3.0f });

        while (!m_Window->ShouldClose()) {
            RenderCommand::SetViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
            RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            RenderCommand::Clear();

            Renderer::BeginScene(camera);

            glm::mat4 model = glm::mat4(1.0f);
            Renderer::Submit(shader, vao, model);

            // submit a second triangle with a different transform (proves “Submit” works)
            glm::mat4 model2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.8f, 0.0f, 0.0f));
            Renderer::Submit(shader, vao, model2);

            Renderer::EndScene();

            m_Window->OnUpdate();
        }
    }

} // namespace Engine