#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/Material.h"   // <-- add this

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        Renderer::Init();
    }

    void Application::Run() {
        // Quad: position + texCoord
        float vertices[] = {
            // x     y     z      u     v
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        auto vao = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(vertices, sizeof(vertices));
        vb->SetLayout({
            { ShaderDataType::Float3 }, // pos
            { ShaderDataType::Float2 }  // uv
            });
        vao->AddVertexBuffer(vb);

        auto ib = std::make_shared<IndexBuffer>(indices, 6);
        vao->SetIndexBuffer(ib);

        // Load GPU resources ONCE
        ShaderLibrary shaders;
        auto texturedShader = shaders.Load("Assets/Shaders/Textured.glsl");
        auto texture = std::make_shared<Texture2D>("Assets/Textures/checker.png");

        auto texturedMat = std::make_shared<Material>(texturedShader);
        texturedMat->SetTexture(0, texture);

        PerspectiveCamera camera(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);
        camera.SetPosition({ 0.0f, 0.0f, 3.0f });

        while (!m_Window->ShouldClose()) {
            RenderCommand::SetViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
            RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            RenderCommand::Clear();

            Renderer::BeginScene(camera);

            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 model2 = glm::translate(glm::mat4(1.0f), glm::vec3(1.1f, 0.0f, 0.0f));

            Renderer::Submit(texturedMat, vao, model);
            Renderer::Submit(texturedMat, vao, model2);

            Renderer::EndScene();
            m_Window->OnUpdate();
        }
    }

} // namespace Engine