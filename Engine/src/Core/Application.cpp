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


#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"

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
        RendererPipeline pipeline;

        // Assets
        ShaderLibrary shaders;
        auto litShader = shaders.Load("Assets/Shaders/Lit.glsl");

        auto model = std::make_shared<Model>("Assets/Models/monkey.obj", litShader);

        // Scene setup
        Scene scene;

        auto monkey = scene.CreateEntity("Monkey");
        monkey.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 0.0f };
        monkey.AddComponent<MeshRendererComponent>(model);

        auto sun = scene.CreateEntity("SunLight");
        sun.AddComponent<DirectionalLightComponent>(
            glm::vec3(0.4f, 0.8f, -0.3f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        auto start = std::chrono::high_resolution_clock::now();
        auto last = start;

        while (!m_Window->ShouldClose()) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            float t = std::chrono::duration<float>(now - start).count();

            camCtrl.OnUpdate(dt);

            // Spin monkey by updating its TransformComponent
            monkey.GetComponent<TransformComponent>().Rotation.y = t;

            scene.OnUpdate(dt);

            pipeline.BeginFrame(m_Window->GetWidth(), m_Window->GetHeight(), camCtrl.GetCamera());

            // Scene does BeginScene/Submit/EndScene internally (for now)
            scene.OnRender(camCtrl.GetCamera());

            // Pipeline presents the offscreen scene to screen
            pipeline.EndFrame();

            m_Window->OnUpdate();
        }
    }
    
}