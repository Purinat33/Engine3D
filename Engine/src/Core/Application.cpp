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

#include "Engine/Assets/AssetManager.h"

#include "Engine/Scene/SceneSerializer.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/ApplicationEvent.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>             

#include <glad/glad.h>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        m_Window->SetEventCallback([this](Event& e) { this->OnEvent(e); });

        Renderer::Init();
    }

    void Application::OnEvent(Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& ev) { return OnWindowClose(ev); });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) { return OnWindowResize(ev); });

        // Optional: log events
        // std::cout << e.ToString() << "\n";
    }

    bool Application::OnWindowClose(WindowCloseEvent&) {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e) {
        // If minimized, skip rendering
        if (e.GetWidth() == 0 || e.GetHeight() == 0)
            return false;
        return false; // not "handled" strictly; pipeline can still resize each frame
    }

    void Application::Run() {
        CameraController camCtrl(1.0472f, 1280.0f / 720.0f, 0.1f, 100.0f);
        RendererPipeline pipeline;

        auto& assets = AssetManager::Get();

        AssetHandle litShaderH = assets.LoadShader("Assets/Shaders/Lit.glsl");
        AssetHandle monkeyModelH = assets.LoadModel("Assets/Models/monkey.obj", litShaderH);

        Scene scene;

        auto monkey = scene.CreateEntity("Monkey");
        monkey.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 0.0f };
        monkey.AddComponent<MeshRendererComponent>(monkeyModelH);

        auto sun = scene.CreateEntity("SunLight");
        sun.AddComponent<DirectionalLightComponent>(
            glm::vec3(0.4f, 0.8f, -0.3f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        SceneSerializer serializer(scene);
        serializer.Serialize("Assets/Scenes/Sandbox.scene");

        // OPTIONAL: test load immediately (into the same scene)
        serializer.Deserialize("Assets/Scenes/Sandbox.scene");

        monkey = scene.FindEntityByTag("Monkey");

        auto start = std::chrono::high_resolution_clock::now();
        auto last = start;

        while (m_Running && !m_Window->ShouldClose()) {
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