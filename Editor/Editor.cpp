#include <Engine/Core/Window.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/RendererPipeline.h>
#include <Engine/Renderer/CameraController.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/SceneSerializer.h>

#include <Engine/Assets/AssetManager.h>

#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/VertexArray.h>
#include <Engine/Renderer/Buffer.h>

#include <Engine/Events/Event.h>
#include <Engine/Events/ApplicationEvent.h>
#include <Engine/Events/KeyEvent.h>
#include <Engine/Events/MouseEvent.h>

#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>

using namespace Engine;

enum class GizmoMode { None, Translate };

static std::shared_ptr<VertexArray> CreateGridPlaneVAO(float halfSize) {
    float v[] = {
        -halfSize, 0.f, -halfSize,
         halfSize, 0.f, -halfSize,
         halfSize, 0.f,  halfSize,
        -halfSize, 0.f,  halfSize
    };

    // OLD: { 0, 1, 2, 2, 3, 0 }  // faces -Y with your winding
    uint32_t idx[] = { 0, 2, 1, 0, 3, 2 }; // faces +Y (visible from above)

    auto vao = std::make_shared<VertexArray>();
    auto vb = std::make_shared<VertexBuffer>(v, sizeof(v));
    vb->SetLayout({ { ShaderDataType::Float3 } });
    vao->AddVertexBuffer(vb);

    auto ib = std::make_shared<IndexBuffer>(idx, 6);
    vao->SetIndexBuffer(ib);
    return vao;
}
int main() {
    auto window = Window::Create({ "Engine3D Editor", 1600, 900 });
    Renderer::Init();

    bool running = true;
    bool ctrlDown = false;

    // Mouse state
    float mouseX = 0.f, mouseY = 0.f;

    // Camera control gating
    bool cameraControl = false;

    // Selection
    uint32_t selectedID = 0;
    Entity selectedEntity{};

    // Gizmo
    GizmoMode gizmo = GizmoMode::None;
    bool dragging = false;
    float dragStartX = 0.f, dragStartY = 0.f;
    glm::vec3 dragStartTranslation{ 0.f };

    // Scene + serializer
    Scene scene;
    SceneSerializer serializer(scene);
    const std::string scenePath = "Assets/Scenes/Sandbox.scene";

    // If scene doesn't exist yet, create default
    if (!serializer.Deserialize(scenePath)) {
        std::cout << "[Editor] No scene found. Creating default...\n";
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
    CameraController editorCam(1.0472f, 1600.0f / 900.0f, 0.1f, 300.0f);
    RendererPipeline pipeline;

    // Grid resources
    auto gridShader = std::make_shared<Shader>("Assets/Shaders/Grid.shader");
    auto gridMat = std::make_shared<Material>(gridShader);
    auto gridVAO = CreateGridPlaneVAO(300.0f);

    // Events
    window->SetEventCallback([&](Event& e) {
        EventDispatcher d(e);

        d.Dispatch<WindowCloseEvent>([&](WindowCloseEvent&) {
            running = false;
            return true;
            });

        d.Dispatch<MouseMovedEvent>([&](MouseMovedEvent& me) {
            mouseX = me.GetX();
            mouseY = me.GetY();
            return false;
            });

        d.Dispatch<MouseButtonPressedEvent>([&](MouseButtonPressedEvent& mb) {
            // RMB -> camera mode
            if (mb.GetMouseButton() == 1) {
                cameraControl = true;
                return true;
            }

            // LMB
            if (mb.GetMouseButton() == 0) {
                // If translate gizmo active & we have a selection, start dragging
                if (gizmo == GizmoMode::Translate && selectedEntity) {
                    dragging = true;
                    dragStartX = mouseX;
                    dragStartY = mouseY;
                    dragStartTranslation = selectedEntity.GetComponent<TransformComponent>().Translation;
                    return true;
                }

                // Otherwise do picking
                uint32_t id = pipeline.ReadPickingID((uint32_t)mouseX, (uint32_t)mouseY);
                selectedID = id;
                pipeline.SetSelectedID(selectedID);

                if (selectedID == 0) {
                    selectedEntity = {};
                    gizmo = GizmoMode::None;
                    dragging = false;
                }
                else {
                    selectedEntity = scene.FindEntityByPickID(selectedID);
                }

                std::cout << "[Pick] ID = " << selectedID << "\n";
                return true;
            }

            return false;
            });

        d.Dispatch<MouseButtonReleasedEvent>([&](MouseButtonReleasedEvent& mb) {
            if (mb.GetMouseButton() == 1) {
                cameraControl = false;
                return true;
            }
            if (mb.GetMouseButton() == 0) {
                dragging = false;
                return true;
            }
            return false;
            });

        d.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& ke) {
            int key = ke.GetKeyCode();

            if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
                ctrlDown = true;

            // Save / Reload
            if (ctrlDown && key == GLFW_KEY_S) {
                std::cout << "[Editor] Save: " << scenePath << "\n";
                serializer.Serialize(scenePath);
                return true;
            }
            if (ctrlDown && key == GLFW_KEY_R) {
                std::cout << "[Editor] Reload: " << scenePath << "\n";
                serializer.Deserialize(scenePath);

                // selection invalid after reload
                selectedID = 0;
                selectedEntity = {};
                pipeline.SetSelectedID(0);
                gizmo = GizmoMode::None;
                dragging = false;

                return true;
            }

            // Gizmo: G = translate
            if (key == GLFW_KEY_G) {
                if (selectedEntity) {
                    gizmo = GizmoMode::Translate;
                    std::cout << "[Gizmo] Translate\n";
                }
                return true;
            }

            // Esc: exit gizmo (or quit if none)
            if (key == GLFW_KEY_ESCAPE) {
                if (gizmo != GizmoMode::None) {
                    gizmo = GizmoMode::None;
                    dragging = false;
                    std::cout << "[Gizmo] None\n";
                    return true;
                }
                else {
                    running = false;
                    return true;
                }
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

        // Camera only active while RMB held
        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);

        // Cursor lock only while camera control is active
        GLFWwindow* native = (GLFWwindow*)window->GetNativeWindow();
        glfwSetInputMode(native, GLFW_CURSOR, cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // If dragging translate gizmo, move selected entity on XZ plane
        if (dragging && gizmo == GizmoMode::Translate && selectedEntity) {
            float dx = mouseX - dragStartX;
            float dy = mouseY - dragStartY;

            // Camera-relative movement projected to XZ plane
            glm::vec3 right = editorCam.GetRight();   right.y = 0.f;
            glm::vec3 fwd = editorCam.GetForward(); fwd.y = 0.f;

            float rl = glm::length(right);
            float fl = glm::length(fwd);
            if (rl > 0.0001f) right /= rl; else right = { 1,0,0 };
            if (fl > 0.0001f) fwd /= fl; else fwd = { 0,0,-1 };

            glm::vec3 camPos = editorCam.GetPosition();
            glm::vec3 objPos = dragStartTranslation;
            float dist = glm::length(objPos - camPos);
            if (dist < 1.0f) dist = 1.0f;

            float scale = 0.0020f * dist; // feel tweak
            glm::vec3 delta = (right * dx + fwd * (-dy)) * scale;

            selectedEntity.GetComponent<TransformComponent>().Translation = dragStartTranslation + delta;
        }

        // ----- Build picking buffer first (for outline + click correctness)
        pipeline.BeginPickingPass(window->GetWidth(), window->GetHeight(), editorCam.GetCamera());
        scene.OnRenderPicking(editorCam.GetCamera(), pipeline.GetIDMaterial());
        pipeline.EndPickingPass();

        // ----- Normal render + present (uses ID buffer + SelectedID for outline)
        pipeline.BeginFrame(window->GetWidth(), window->GetHeight(), editorCam.GetCamera());

        // Grid draw first (so meshes draw over it)
        gridShader->Bind();
        gridShader->SetFloat("u_GridScale", 1.0f); // if you don’t have SetFloat, use SetFloat3/SetFloat4 pattern or add SetFloat
        gridShader->SetFloat3("u_GridColor", 0.45f, 0.45f, 0.45f);
        gridShader->SetFloat3("u_BaseColor", 0.12f, 0.12f, 0.12f);
        Renderer::Submit(gridMat, gridVAO, glm::mat4(1.0f));

        scene.OnUpdate(dt);
        scene.OnRender(editorCam.GetCamera()); // submits scene meshes

        pipeline.EndFrame();
        window->OnUpdate();
    }

    return 0;
}