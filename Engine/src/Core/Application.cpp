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

#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/ScreenQuad.h"

#include "Engine/Renderer/RendererPipeline.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>             

#include <glad/glad.h>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        Renderer::Init();
    }

    void Application::Run() {
        CameraController camCtrl(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);

        ShaderLibrary shaders;
        auto litShader = shaders.Load("Assets/Shaders/Lit.glsl");
        

        Model model3d("Assets/Models/monkey.obj", litShader);

        auto start = std::chrono::high_resolution_clock::now();
        auto last = start;

        RendererPipeline pipeline;

        while (!m_Window->ShouldClose()) {

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            float t = std::chrono::duration<float>(now - start).count();

            camCtrl.OnUpdate(dt);

            glm::mat4 modelM(1.0f);
            modelM = glm::rotate(modelM, t, glm::vec3(0.0f, 1.0f, 0.0f));

            // Begin pipeline frame (does: bind scene FB, clear, BeginScene(camera))
            pipeline.BeginFrame(m_Window->GetWidth(), m_Window->GetHeight(), camCtrl.GetCamera());

            // Light params — set once per frame (shader is used by model materials)
            litShader->Bind();
            litShader->SetFloat3("u_LightDir", 0.4f, 0.8f, -0.3f);
            litShader->SetFloat3("u_LightColor", 1.0f, 1.0f, 1.0f);

            for (const auto& sm : model3d.GetSubMeshes())
                Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), modelM);

            // End pipeline frame (does: EndScene, bind default FB, draw screen quad)
            pipeline.EndFrame();

            m_Window->OnUpdate();
        }
        
    }
}