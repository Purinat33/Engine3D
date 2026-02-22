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
#include "Engine/Renderer/CameraController.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>                         

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        Renderer::Init();
    }

    void Application::Run() {
        CameraController camCtrl(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);

        ShaderLibrary shaders;
        auto litShader = shaders.Load("Assets/Shaders/Lit.glsl"); // or .glsl
        Model model3d("Assets/Models/monkey.obj", litShader);


        auto start = std::chrono::high_resolution_clock::now();
        auto last = start;

        while (!m_Window->ShouldClose()) {

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            float t = std::chrono::duration<float>(now - start).count();

            camCtrl.OnUpdate(dt);

            RenderCommand::SetViewport(0, 0, m_Window->GetWidth(), m_Window->GetHeight());
            RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            RenderCommand::Clear();

            // Use camera controller camera:
            Renderer::BeginScene(camCtrl.GetCamera());

            litShader->Bind();
            litShader->SetFloat3("u_LightDir", 0.4f, 0.8f, -0.3f);
            litShader->SetFloat3("u_LightColor", 1.0f, 1.0f, 1.0f);

            glm::mat4 modelM(1.0f);
            modelM = glm::rotate(modelM, t, glm::vec3(0.0f, 1.0f, 0.0f));

            for (const auto& sm : model3d.GetSubMeshes()) {
                Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), modelM);
            }

            Renderer::EndScene();
            m_Window->OnUpdate();
        }
        
    }
}