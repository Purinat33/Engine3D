#include <Engine/Core/Window.h>

#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/RendererPipeline.h>
#include <Engine/Renderer/CameraController.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/SceneSerializer.h>

#include <Engine/Assets/AssetManager.h>
#include <Engine/Project/ProjectSettings.h>

#include <Engine/Events/Event.h>
#include <Engine/Events/ApplicationEvent.h>
#include <Engine/Events/WindowFocusEvent.h>
#include <Engine/Events/KeyEvent.h>

#include "Engine/Renderer/TextureCube.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <filesystem>
#include <iostream>

using namespace Engine;

int main() {
    auto window = Window::Create({ "Engine3D - Sandbox", 1280, 720 });

    Renderer::Init();
    Renderer::SetSkybox(std::make_shared<TextureCube>(std::array<std::string, 6>{
        "Assets/Skybox/px.png",
            "Assets/Skybox/nx.png",
            "Assets/Skybox/py.png",
            "Assets/Skybox/ny.png",
            "Assets/Skybox/pz.png",
            "Assets/Skybox/nz.png"
    }));
    std::cout << "[Sandbox] Skybox set\n";
    RendererPipeline pipeline;

    // --- State ---
    bool running = true;
    bool hasFocus = true;
    bool captureMouse = true; // game mode default

    // Capture immediately
    window->SetCursorMode(true);

    // Camera
    CameraController cam(1.0472f, 1280.0f / 720.0f, 0.1f, 300.0f);
    cam.SetTransform(glm::vec3(0.0f, 2.0f, 6.0f), -3.1415926f, -0.2f);
    cam.SetActive(true);

    // --- Events (focus + ESC) ---
    window->SetEventCallback([&](Event& e) {
        EventDispatcher d(e);

        d.Dispatch<WindowCloseEvent>([&](WindowCloseEvent&) {
            running = false;
            return true;
            });

        d.Dispatch<WindowFocusEvent>([&](WindowFocusEvent& ev) {
            hasFocus = ev.IsFocused();

            if (!hasFocus) {
                window->SetCursorMode(false);
                cam.SetActive(false);
                cam.OnUpdate(0.0f); // reset first mouse
            }
            else {
                window->SetCursorMode(captureMouse);
                cam.SetActive(captureMouse);
                cam.OnUpdate(0.0f);
            }
            return false;
            });

        d.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& ke) {
            if (ke.GetKeyCode() == GLFW_KEY_ESCAPE && hasFocus) {
                captureMouse = !captureMouse;
                window->SetCursorMode(captureMouse);
                cam.SetActive(captureMouse);
                cam.OnUpdate(0.0f);
                return true;
            }
            return false;
            });

        return;
        });

    // --- Scene load ---
    Scene scene;
    SceneSerializer serializer(scene);

    const std::string fallbackScene = "Assets/Scenes/Sandbox.scene";
    const std::string startupScenePath = ProjectSettings::GetStartupSceneOrDefault(fallbackScene);

    if (!serializer.Deserialize(startupScenePath)) {
        std::cout << "[Sandbox] Failed to load: " << startupScenePath << "\n";
        std::cout << "[Sandbox] cwd=" << std::filesystem::current_path().string() << "\n";
        std::cout << "[Sandbox] Creating default scene...\n";

        auto& assets = AssetManager::Get();
        AssetHandle litShaderH = assets.LoadShader("Assets/Shaders/Lit.glsl");
        AssetHandle monkeyModelH = assets.LoadModel("Assets/Models/monkey.obj", litShaderH);

        auto monkey = scene.CreateEntity("Monkey");
        monkey.AddComponent<MeshRendererComponent>(monkeyModelH);

        auto sun = scene.CreateEntity("SunLight");
        auto& dl = sun.AddComponent<DirectionalLightComponent>();
        dl.Direction = glm::vec3(0.4f, 0.8f, -0.3f);
        dl.Color = glm::vec3(1.0f);

        serializer.Serialize(fallbackScene);
        std::cout << "[Sandbox] Wrote default scene to: " << fallbackScene << "\n";
    }
    else {
        std::cout << "[Sandbox] Loaded startup scene: " << startupScenePath << "\n";
    }

    // --- APPLY SPAWN POINT (if any) ---
    {
        auto view = scene.Registry().view<TagComponent, TransformComponent, SpawnPointComponent>();

        entt::entity chosen = entt::null;
        for (auto e : view) {
            const auto& tag = view.get<TagComponent>(e).Tag;
            if (tag == "SpawnPoint") { chosen = e; break; }
            if (chosen == entt::null) chosen = e; // fallback to first
        }

        if (chosen != entt::null) {
            auto& tc = view.get<TransformComponent>(chosen);
            std::cout << "[Sandbox] SpawnPoint pos=("
                << tc.Translation.x << "," << tc.Translation.y << "," << tc.Translation.z
                << ") rot(p,y)=(" << tc.Rotation.x << "," << tc.Rotation.y << ")\n";

            // SetTransform(position, yaw, pitch)
            cam.SetTransform(tc.Translation, tc.Rotation.y, tc.Rotation.x);
        }
        else {
            std::cout << "[Sandbox] No SpawnPoint found.\n";
        }
    }

    auto last = std::chrono::high_resolution_clock::now();

    while (running && !window->ShouldClose()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (!hasFocus) {
            window->OnUpdate();
            continue;
        }

        if (dt > 0.1f) dt = 0.1f;

        if (captureMouse) {
            cam.SetActive(true);
            cam.OnUpdate(dt);
        }
        else {
            cam.SetActive(false);
            cam.OnUpdate(0.0f);
        }

        uint32_t w = window->GetWidth();
        uint32_t h = window->GetHeight();
        if (w == 0 || h == 0) {
            window->OnUpdate();
            continue;
        }

        pipeline.BeginScenePass(w, h, cam.GetCamera());
        scene.OnUpdate(dt);
        scene.OnRender(cam.GetCamera()); // submits only
        pipeline.EndScenePass();

        pipeline.PresentToScreen();
        window->OnUpdate();
    }

    return 0;
}