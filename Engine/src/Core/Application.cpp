#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Shader.h"       
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Model.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>                         

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        Renderer::Init();
    }

    void Application::Run() {
        ShaderLibrary shaders;
        auto litShader = shaders.Load("Assets/Shaders/Lit.glsl");
        auto litMat = std::make_shared<Material>(litShader);

        Model model3d("Assets/Models/monkey.obj");

        PerspectiveCamera camera(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);
        camera.SetPosition({ 0.0f, 0.0f, 3.0f });

        auto start = std::chrono::high_resolution_clock::now();

        while (!m_Window->ShouldClose()) {
            RenderCommand::SetViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
            RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            RenderCommand::Clear();

            Renderer::BeginScene(camera);

            // time in seconds
            auto now = std::chrono::high_resolution_clock::now();
            float t = std::chrono::duration<float>(now - start).count();

            // Light params
            litShader->Bind();
            litShader->SetFloat3("u_LightDir", 0.4f, 0.8f, -0.3f);
            litShader->SetFloat3("u_LightColor", 1.0f, 1.0f, 1.0f);
            litShader->SetFloat3("u_BaseColor", 0.9f, 0.7f, 0.2f);

            glm::mat4 modelM(1.0f);
            modelM = glm::rotate(modelM, t, glm::vec3(0.0f, 1.0f, 0.0f));

            for (auto& mesh : model3d.GetMeshes()) {
                Renderer::Submit(litMat, mesh->GetVertexArray(), modelM);
            }

            Renderer::EndScene();
            m_Window->OnUpdate();
        }
    }
}