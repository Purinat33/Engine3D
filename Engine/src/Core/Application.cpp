#include "pch.h"
#include "Engine/Core/Application.h"

#include "Engine/Core/Window.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/RendererPipeline.h"
#include "Engine/Renderer/CameraController.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/SceneSerializer.h"

#include "Engine/Events/Event.h"
#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/WindowFocusEvent.h"
#include "Engine/Events/KeyEvent.h"

#include "Engine/Renderer/TextureCube.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>
#include <iostream>

namespace Engine {

    Application::Application() {
        m_Window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });
        m_Window->SetEventCallback([this](Event& e) { this->OnEvent(e); });

        Renderer::Init();

        m_CaptureMouse = true;
        m_Window->SetCursorMode(true);
    }

    void Application::OnEvent(Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) { m_Running = false; return true; });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent&) { return false; });

        dispatcher.Dispatch<WindowFocusEvent>([this](WindowFocusEvent& ev) {
            m_HasFocus = ev.IsFocused();

            if (!m_HasFocus) {
                m_CaptureMouse = false;
                m_Window->SetCursorMode(false);
            }
            else {
                m_CaptureMouse = true;
                m_Window->SetCursorMode(true);
            }
            return false;
            });

        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ke) {
            if (ke.GetKeyCode() == GLFW_KEY_ESCAPE) {
                m_CaptureMouse = !m_CaptureMouse;
                m_Window->SetCursorMode(m_CaptureMouse);
                return true;
            }
            return false;
            });
    }

    void Application::Run() {
        CameraController camCtrl(1.0472f, 1280.0f / 720.0f, 0.1f, 300.0f);
        RendererPipeline pipeline;

        Scene scene;
        SceneSerializer serializer(scene);

        const std::string scenePath = "Assets/Scenes/Sandbox.scene";
        if (!serializer.Deserialize(scenePath)) {
            std::cerr << "[Sandbox] Failed to load: " << scenePath
                << " (cwd=" << std::filesystem::current_path().string() << ")\n";
        }

        // Skybox for this scene (load once)
        Renderer::SetSkybox(std::make_shared<TextureCube>(std::array<std::string, 6>{
            "Assets/Skybox/px.png",
                "Assets/Skybox/nx.png",
                "Assets/Skybox/py.png",
                "Assets/Skybox/ny.png",
                "Assets/Skybox/pz.png",
                "Assets/Skybox/nz.png"
        }));
        std::cout << "[Sandbox] Skybox set\n";
        // --- APPLY SPAWN POINT (if any) ---
        {
            auto view = scene.Registry().view<TagComponent, TransformComponent, SpawnPointComponent>();

            entt::entity chosen = entt::null;
            for (auto e : view) {
                const auto& tag = view.get<TagComponent>(e).Tag;
                if (tag == "SpawnPoint") { chosen = e; break; }
                if (chosen == entt::null) chosen = e;
            }

            if (chosen != entt::null) {
                auto& tc = view.get<TransformComponent>(chosen);
                std::cout << "[Sandbox] SpawnPoint pos=("
                    << tc.Translation.x << "," << tc.Translation.y << "," << tc.Translation.z
                    << ") rot(p,y)=(" << tc.Rotation.x << "," << tc.Rotation.y << ")\n";

                camCtrl.SetTransform(tc.Translation, tc.Rotation.y, tc.Rotation.x);
            }
            else {
                std::cout << "[Sandbox] No SpawnPoint found.\n";
            }
        }

        auto last = std::chrono::high_resolution_clock::now();

        while (m_Running && !m_Window->ShouldClose()) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            if (!m_HasFocus) {
                m_Window->OnUpdate();
                continue;
            }

            if (dt > 0.1f) dt = 0.1f;

            camCtrl.SetActive(m_CaptureMouse);
            camCtrl.OnUpdate(m_CaptureMouse ? dt : 0.0f);

            uint32_t w = m_Window->GetWidth();
            uint32_t h = m_Window->GetHeight();
            if (w == 0 || h == 0) {
                m_Window->OnUpdate();
                continue;
            }

            pipeline.BeginScenePass(w, h, camCtrl.GetCamera());
            scene.OnUpdate(dt);
            scene.OnRender(camCtrl.GetCamera()); // submits only + pushes light inside Scene
            pipeline.EndScenePass();
            pipeline.PresentToScreen();

            m_Window->OnUpdate();
        }
    }

} // namespace Engine