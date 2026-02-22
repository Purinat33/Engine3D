#include <Engine/Core/Window.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/RendererPipeline.h>
#include <Engine/Renderer/CameraController.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/SceneSerializer.h>

#include <Engine/Assets/AssetManager.h>

#include <Engine/Events/Event.h>
#include <Engine/Events/ApplicationEvent.h>
#include <Engine/Events/KeyEvent.h>

#include <GLFW/glfw3.h> // for GLFW_KEY_* codes

#include <chrono>
#include <iostream>

int main() {
    using namespace Engine;

    // Create window (GL context loads in WindowsWindow)
    auto window = Window::Create({ "Engine3D Editor", 1600, 900 });

    // Renderer init must happen AFTER GL context exists
    Renderer::Init();

    bool running = true;
    bool ctrlDown = false;

    // Scene + serializer
    Scene scene;
    SceneSerializer serializer(scene);

    const std::string scenePath = "Assets/Scenes/Sandbox.scene";

    // If scene doesn't exist yet, create a default one
    bool loaded = serializer.Deserialize(scenePath);
    if (!loaded) {
        std::cout << "[Editor] No scene found. Creating default scene...\n";

        auto& assets = AssetManager::Get();
        AssetHandle litShaderH = assets.LoadShader("Assets/Shaders/Lit.glsl");
        AssetHandle monkeyModelH = assets.LoadModel("Assets/Models/monkey.obj", litShaderH);

        auto monkey = scene.CreateEntity("Monkey");
        monkey.AddComponent<MeshRendererComponent>(monkeyModelH);

        auto sun = scene.CreateEntity("SunLight");
        sun.AddComponent<DirectionalLightComponent>(
            glm::vec3(0.4f, 0.8f, -0.3f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        serializer.Serialize(scenePath);
    }

    // Editor camera + pipeline
    CameraController editorCam(1.0472f, 1600.0f / 900.0f, 0.1f, 200.0f);
    RendererPipeline pipeline;

    // Hook events
    window->SetEventCallback([&](Event& e) {
        EventDispatcher d(e);

        d.Dispatch<WindowCloseEvent>([&](WindowCloseEvent&) {
            running = false;
            return true;
            });

        d.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& ke) {
            int key = ke.GetKeyCode();

            if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
                ctrlDown = true;

            // Ctrl+S: save
            if (ctrlDown && key == GLFW_KEY_S) {
                std::cout << "[Editor] Save: " << scenePath << "\n";
                serializer.Serialize(scenePath);
                return true;
            }

            // Ctrl+R: reload
            if (ctrlDown && key == GLFW_KEY_R) {
                std::cout << "[Editor] Reload: " << scenePath << "\n";
                serializer.Deserialize(scenePath);
                return true;
            }

            // Esc: exit
            if (key == GLFW_KEY_ESCAPE) {
                running = false;
                return true;
            }

            return false;
            });

        d.Dispatch<KeyReleasedEvent>([&](KeyReleasedEvent& ke) {
            int key = ke.GetKeyCode();
            if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
                ctrlDown = false;
            return false;
            });
        });

    // Timing
    auto last = std::chrono::high_resolution_clock::now();

    while (running && !window->ShouldClose()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        // Update editor camera (WASD + mouse)
        editorCam.OnUpdate(dt);

        // Render
        pipeline.BeginFrame(window->GetWidth(), window->GetHeight(), editorCam.GetCamera());

        // Scene submits (no Begin/End inside scene)
        scene.OnUpdate(dt);
        scene.OnRender(editorCam.GetCamera());

        pipeline.EndFrame();
        window->OnUpdate();
    }

    return 0;
}