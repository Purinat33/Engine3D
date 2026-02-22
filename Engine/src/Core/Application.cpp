#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/ShaderLibrary.h"

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

        ShaderLibrary shaders;
        auto shader = shaders.Load("Assets/Shaders/FlatColor.glsl");

        PerspectiveCamera camera(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);
        camera.SetPosition({ 0.0f, 0.0f, 3.0f });

        while (!m_Window->ShouldClose()) {
            RenderCommand::SetViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
            RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            RenderCommand::Clear();

            Renderer::BeginScene(camera);

            glm::mat4 model = glm::mat4(1.0f);
            Renderer::Submit(shader, vao, model, { 0.8f, 0.2f, 0.4f, 1.0f });

            // submit a second triangle with a different transform (proves “Submit” works)
            glm::mat4 model2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.8f, 0.0f, 0.0f));
            Renderer::Submit(shader, vao, model2, { 0.2f, 0.8f, 0.4f, 1.0f });
            

            Renderer::EndScene();

            m_Window->OnUpdate();
        }
    }

} // namespace Engine