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
#include <cmath>

using namespace Engine;

enum class GizmoMode { None, Translate, Rotate, Scale };

static std::shared_ptr<VertexArray> CreateGridPlaneVAO(float halfSize) {
    float v[] = {
        -halfSize, 0.f, -halfSize,
         halfSize, 0.f, -halfSize,
         halfSize, 0.f,  halfSize,
        -halfSize, 0.f,  halfSize
    };
    uint32_t idx[] = {
    0, 1, 2, 0, 2, 3    // -Y facing
    };

    auto vao = std::make_shared<VertexArray>();
    auto vb = std::make_shared<VertexBuffer>(v, sizeof(v));
    vb->SetLayout({ { ShaderDataType::Float3 } });
    vao->AddVertexBuffer(vb);

    auto ib = std::make_shared<IndexBuffer>(idx, 6);
    vao->SetIndexBuffer(ib);
    return vao;
}

// Same folding you use in Scene picking
static uint32_t FoldUUIDToPickID(UUID id) {
    uint32_t v = (uint32_t)(id ^ (id >> 32));
    return v == 0 ? 1u : v;
}

static float SnapFloat(float v, float step) {
    if (step <= 0.0f) return v;
    return std::round(v / step) * step;
}

int main() {
    auto window = Window::Create({ "Engine3D Editor", 1600, 900 });
    Renderer::Init();

    bool running = true;
    bool ctrlDown = false;

    float mouseX = 0.f, mouseY = 0.f;

    bool cameraControl = false;

    uint32_t selectedID = 0;
    Entity selectedEntity{};

    GizmoMode gizmo = GizmoMode::None;
    bool dragging = false;

    float dragStartX = 0.f, dragStartY = 0.f;
    glm::vec3 dragStartTranslation{ 0.f };
    glm::vec3 dragStartRotation{ 0.f };
    glm::vec3 dragStartScale{ 1.f };

    // Scene + serializer
    Scene scene;
    SceneSerializer serializer(scene);
    const std::string scenePath = "Assets/Scenes/Sandbox.scene";

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

    CameraController editorCam(1.0472f, 1600.0f / 900.0f, 0.1f, 300.0f);
    editorCam.SetTransform(glm::vec3(0.0f, 8.0f, 8.0f), -3.1415926f, -0.75f);
    RendererPipeline pipeline;

    // Grid
    auto gridShader = std::make_shared<Shader>("Assets/Shaders/Grid.shader");
    auto gridMat = std::make_shared<Material>(gridShader);
    gridMat->SetTwoSided(true);
    auto gridVAO = CreateGridPlaneVAO(100.0f);

    auto updateSelection = [&](uint32_t pickID) {
        selectedID = pickID;
        pipeline.SetSelectedID(selectedID);

        if (selectedID == 0) {
            selectedEntity = {};
            gizmo = GizmoMode::None;
            dragging = false;
            return;
        }
        selectedEntity = scene.FindEntityByPickID(selectedID);
        if (!selectedEntity) {
            selectedID = 0;
            pipeline.SetSelectedID(0);
        }
        };

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
            // RMB = camera mode
            if (mb.GetMouseButton() == 1) {
                cameraControl = true;
                return true;
            }

            // LMB
            if (mb.GetMouseButton() == 0) {
                // If gizmo active and selected, start dragging
                if (gizmo != GizmoMode::None && selectedEntity) {
                    dragging = true;
                    dragStartX = mouseX;
                    dragStartY = mouseY;

                    auto& tc = selectedEntity.GetComponent<TransformComponent>();
                    dragStartTranslation = tc.Translation;
                    dragStartRotation = tc.Rotation;
                    dragStartScale = tc.Scale;
                    return true;
                }

                // Otherwise do picking
                uint32_t id = pipeline.ReadPickingID((uint32_t)mouseX, (uint32_t)mouseY);
                updateSelection(id);
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
                updateSelection(0);
                return true;
            }

            // Duplicate selected: Ctrl + D
            if (ctrlDown && key == GLFW_KEY_D) {
                if (selectedEntity) {
                    Entity copy = scene.DuplicateEntity(selectedEntity);
                    if (copy) {
                        // Select new copy
                        uint32_t id = FoldUUIDToPickID(copy.GetComponent<IDComponent>().ID);
                        updateSelection(id);
                        std::cout << "[Editor] Duplicated entity. New PickID=" << id << "\n";
                    }
                }
                return true;
            }

            // Delete selected
            if (key == GLFW_KEY_DELETE) {
                if (selectedEntity) {
                    scene.DestroyEntity(selectedEntity);
                    updateSelection(0);
                    std::cout << "[Editor] Deleted entity.\n";
                }
                return true;
            }

            // Don’t switch gizmo while flying camera
            if (cameraControl) return false;

            // Gizmo modes
            if (key == GLFW_KEY_G) {
                gizmo = selectedEntity ? GizmoMode::Translate : GizmoMode::None;
                dragging = false;
                std::cout << "[Gizmo] Translate\n";
                return true;
            }
            if (key == GLFW_KEY_R) {
                gizmo = selectedEntity ? GizmoMode::Rotate : GizmoMode::None;
                dragging = false;
                std::cout << "[Gizmo] Rotate\n";
                return true;
            }
            if (key == GLFW_KEY_F) {
                gizmo = selectedEntity ? GizmoMode::Scale : GizmoMode::None;
                dragging = false;
                std::cout << "[Gizmo] Scale\n";
                return true;
            }

            // Esc: cancel gizmo or exit
            if (key == GLFW_KEY_ESCAPE) {
                if (gizmo != GizmoMode::None) {
                    gizmo = GizmoMode::None;
                    dragging = false;
                    std::cout << "[Gizmo] None\n";
                    return true;
                }
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

    auto last = std::chrono::high_resolution_clock::now();

    while (running && !window->ShouldClose()) {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        // Camera active only when RMB held
        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);

        GLFWwindow* native = (GLFWwindow*)window->GetNativeWindow();
        glfwSetInputMode(native, GLFW_CURSOR, cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // Apply gizmo drag
        if (dragging && gizmo != GizmoMode::None && selectedEntity) {
            float dx = mouseX - dragStartX;
            float dy = mouseY - dragStartY;

            auto& tc = selectedEntity.GetComponent<TransformComponent>();

            bool snap = ctrlDown;

            if (gizmo == GizmoMode::Translate) {
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

                float scale = 0.0020f * dist;
                glm::vec3 delta = (right * dx + fwd * (-dy)) * scale;

                glm::vec3 out = dragStartTranslation + delta;

                if (snap) {
                    const float step = 0.5f;
                    out.x = SnapFloat(out.x, step);
                    out.z = SnapFloat(out.z, step);
                }

                tc.Translation = out;
            }
            else if (gizmo == GizmoMode::Rotate) {
                // Yaw rotation from mouse X
                float speed = 0.01f;
                float yaw = dragStartRotation.y + dx * speed;

                if (snap) {
                    // 15 degrees
                    float step = 3.1415926f / 12.0f;
                    yaw = SnapFloat(yaw, step);
                }

                tc.Rotation.y = yaw;
            }
            else if (gizmo == GizmoMode::Scale) {
                // Uniform scale from mouse Y
                float speed = 0.01f;
                float s = 1.0f + (-dy) * speed;
                if (s < 0.05f) s = 0.05f;

                glm::vec3 out = dragStartScale * s;

                if (snap) {
                    const float step = 0.1f;
                    out.x = SnapFloat(out.x, step);
                    out.y = SnapFloat(out.y, step);
                    out.z = SnapFloat(out.z, step);
                }

                tc.Scale = out;
            }
        }

        // Picking pass
        pipeline.BeginPickingPass(window->GetWidth(), window->GetHeight(), editorCam.GetCamera());
        scene.OnRenderPicking(editorCam.GetCamera(), pipeline.GetIDMaterial());
        pipeline.EndPickingPass();

        // Normal render (outline uses ID buffer + selectedID)
        //pipeline.BeginFrame(window->GetWidth(), window->GetHeight(), editorCam.GetCamera());

        //// Grid
        //gridShader->Bind();
        //gridShader->SetFloat("u_GridScale", 1.0f);
        //gridShader->SetFloat3("u_GridColor", 0.45f, 0.45f, 0.45f);
        //gridShader->SetFloat3("u_BaseColor", 0.12f, 0.12f, 0.12f);
        //gridShader->SetFloat("u_Opacity", 0.30f);
        //Renderer::Submit(gridMat, gridVAO, glm::mat4(1.0f));

        //scene.OnUpdate(dt);
        //scene.OnRender(editorCam.GetCamera());

        //pipeline.EndFrame();
        window->OnUpdate();
    }

    return 0;
}