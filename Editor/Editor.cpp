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

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

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
    uint32_t idx[] = { 0, 1, 2, 2, 3, 0 };

    auto vao = std::make_shared<VertexArray>();
    auto vb = std::make_shared<VertexBuffer>(v, sizeof(v));
    vb->SetLayout({ { ShaderDataType::Float3 } });
    vao->AddVertexBuffer(vb);

    auto ib = std::make_shared<IndexBuffer>(idx, 6);
    vao->SetIndexBuffer(ib);
    return vao;
}

static uint32_t FoldUUIDToPickID(UUID id) {
    uint32_t v = (uint32_t)(id ^ (id >> 32));
    return v == 0 ? 1u : v;
}

static float SnapFloat(float v, float step) {
    if (step <= 0.0f) return v;
    return std::round(v / step) * step;
}

int main() {
    auto window = Window::Create({ "Engine3D Editor (ImGui)", 1600, 900 });
    GLFWwindow* native = (GLFWwindow*)window->GetNativeWindow();

    Renderer::Init();

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(native, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- Scene ---
    Scene scene;
    SceneSerializer serializer(scene);
    const std::string scenePath = "Assets/Scenes/Sandbox.scene";

    if (!serializer.Deserialize(scenePath)) {
        auto& assets = AssetManager::Get();
        AssetHandle litShaderH = assets.LoadShader("Assets/Shaders/Lit.glsl");
        AssetHandle monkeyModelH = assets.LoadModel("Assets/Models/monkey.obj", litShaderH);

        auto monkey = scene.CreateEntity("Monkey");
        monkey.AddComponent<MeshRendererComponent>(monkeyModelH);

        auto sun = scene.CreateEntity("SunLight");
        auto& dl = sun.AddComponent<DirectionalLightComponent>();
        dl.Direction = glm::vec3(0.4f, 0.8f, -0.3f);
        dl.Color = glm::vec3(1.0f, 1.0f, 1.0f);


        serializer.Serialize(scenePath);
    }

    // --- Pipeline + camera ---
    RendererPipeline pipeline;

    CameraController editorCam(1.0472f, 1600.0f / 900.0f, 0.1f, 300.0f);
    editorCam.SetTransform(glm::vec3(0.0f, 8.0f, 8.0f), -3.1415926f, -0.75f);

    // Grid
    auto gridShader = std::make_shared<Shader>("Assets/Shaders/Grid.shader");
    auto gridMat = std::make_shared<Material>(gridShader);
    gridMat->SetTwoSided(true);
    auto gridVAO = CreateGridPlaneVAO(100.0f);

    // Editor state
    uint32_t selectedID = 0;
    Entity selectedEntity{};
    GizmoMode gizmo = GizmoMode::None;

    bool dragging = false;
    float dragStartX = 0.f, dragStartY = 0.f;
    glm::vec3 dragStartTranslation{ 0.f };
    glm::vec3 dragStartRotation{ 0.f };
    glm::vec3 dragStartScale{ 1.f };

    // Viewport info
    ImVec2 viewportPos{ 0,0 };
    ImVec2 viewportSize{ 1,1 };
    bool viewportHovered = false;
    bool viewportFocused = false;

    auto start = std::chrono::high_resolution_clock::now();
    auto last = start;

    while (!window->ShouldClose()) {
        glfwPollEvents();

        // Timing
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        // --- Begin ImGui frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Dockspace ---
        ImGuiWindowFlags dockFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* mainVp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainVp->Pos);
        ImGui::SetNextWindowSize(mainVp->Size);
        ImGui::SetNextWindowViewport(mainVp->ID);
        dockFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        dockFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("DockSpace", nullptr, dockFlags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspaceID = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspaceID, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

        // Menu
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save", "Ctrl+S")) serializer.Serialize(scenePath);
                if (ImGui::MenuItem("Reload", "Ctrl+R")) {
                    serializer.Deserialize(scenePath);
                    selectedID = 0;
                    selectedEntity = {};
                    pipeline.SetSelectedID(0);
                }
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(native, 1);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End(); // DockSpace root

        // --- Hierarchy ---
        ImGui::Begin("Hierarchy");
        auto view = scene.Registry().view<IDComponent, TagComponent>();
        view.each([&](auto ent, IDComponent& idc, TagComponent& tc) {
            uint32_t pid = FoldUUIDToPickID(idc.ID);
            bool selected = (pid == selectedID);

            if (ImGui::Selectable(tc.Tag.c_str(), selected)) {
                selectedID = pid;
                pipeline.SetSelectedID(selectedID);
                selectedEntity = scene.FindEntityByPickID(selectedID);
            }
            });

        if (ImGui::Button("Clear Selection")) {
            selectedID = 0;
            selectedEntity = {};
            pipeline.SetSelectedID(0);
            gizmo = GizmoMode::None;
            dragging = false;
        }
        ImGui::End();

        // --- Inspector ---
        ImGui::Begin("Inspector");
        if (selectedEntity) {
            auto& tag = selectedEntity.GetComponent<TagComponent>().Tag;
            ImGui::Text("Entity: %s", tag.c_str());
            ImGui::Text("PickID: %u", selectedID);

            auto& tr = selectedEntity.GetComponent<TransformComponent>();

            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Translation", &tr.Translation.x, 0.05f);

            // Show rotation in degrees but store radians
            float rotDeg[3] = {
                tr.Rotation.x * 57.29578f,
                tr.Rotation.y * 57.29578f,
                tr.Rotation.z * 57.29578f
            };
            if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f)) {
                tr.Rotation.x = rotDeg[0] * 0.01745329f;
                tr.Rotation.y = rotDeg[1] * 0.01745329f;
                tr.Rotation.z = rotDeg[2] * 0.01745329f;
            }

            ImGui::DragFloat3("Scale", &tr.Scale.x, 0.02f);

            ImGui::Separator();
            ImGui::Text("Gizmo");
            if (ImGui::Button("Translate (G)")) gizmo = GizmoMode::Translate;
            ImGui::SameLine();
            if (ImGui::Button("Rotate (R)")) gizmo = GizmoMode::Rotate;
            ImGui::SameLine();
            if (ImGui::Button("Scale (F)")) gizmo = GizmoMode::Scale;
            if (ImGui::Button("None (Esc)")) { gizmo = GizmoMode::None; dragging = false; }
            ImGui::Text("Hold Ctrl to snap while dragging.");
        }
        else {
            ImGui::Text("No selection.");
        }
        ImGui::End();

        // --- Viewport ---
        ImGui::Begin("Viewport");
        viewportPos = ImGui::GetCursorScreenPos();
        viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x < 1) viewportSize.x = 1;
        if (viewportSize.y < 1) viewportSize.y = 1;

        // Render passes at viewport resolution
        uint32_t vw = (uint32_t)viewportSize.x;
        uint32_t vh = (uint32_t)viewportSize.y;

        // Camera control only when viewport hovered + RMB and ImGui not capturing mouse
        viewportHovered = ImGui::IsWindowHovered();
        viewportFocused = ImGui::IsWindowFocused();

        bool cameraControl = viewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse;
        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);
        glfwSetInputMode(native, GLFW_CURSOR, cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // Hotkeys for gizmo while viewport focused
        if (viewportFocused && !io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_G) && selectedEntity) gizmo = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_R) && selectedEntity) gizmo = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_F) && selectedEntity) gizmo = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gizmo = GizmoMode::None; dragging = false; }
        }

        // --- Picking pass (needed every frame for click + outline) ---
        pipeline.BeginPickingPass(vw, vh, editorCam.GetCamera());
        scene.OnRenderPicking(editorCam.GetCamera(), pipeline.GetIDMaterial());
        pipeline.EndPickingPass();

        // --- Scene pass ---
        pipeline.BeginScenePass(vw, vh, editorCam.GetCamera());

        // Grid uniforms (set once; persists)
        gridShader->Bind();
        gridShader->SetFloat("u_GridScale", 1.0f);
        gridShader->SetFloat3("u_GridColor", 0.45f, 0.45f, 0.45f);
        gridShader->SetFloat3("u_BaseColor", 0.12f, 0.12f, 0.12f);
        gridShader->SetFloat("u_Opacity", 0.30f);
        Renderer::Submit(gridMat, gridVAO, glm::mat4(1.0f));

        scene.OnUpdate(dt);
        scene.OnRender(editorCam.GetCamera());
        pipeline.EndScenePass();

        // Compose scene+outline into composite texture for ImGui
        pipeline.Compose();

        // Display in ImGui (flip UV because OpenGL texture origin)
        ImTextureID tex = (ImTextureID)(intptr_t)pipeline.GetCompositeTexture();
        ImGui::Image(tex, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        bool imageHovered = ImGui::IsItemHovered();

        // Viewport mouse position (relative to viewport image)
        ImVec2 mp = ImGui::GetMousePos();
        float mx = mp.x - viewportPos.x;
        float my = mp.y - viewportPos.y;

        // Gizmo dragging / picking on LMB (only inside viewport image)
        bool ctrlDown = io.KeyCtrl;

        if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse) {
            // If gizmo active and selected -> start dragging, else pick
            if (gizmo != GizmoMode::None && selectedEntity) {
                dragging = true;
                dragStartX = mx;
                dragStartY = my;
                auto& tc = selectedEntity.GetComponent<TransformComponent>();
                dragStartTranslation = tc.Translation;
                dragStartRotation = tc.Rotation;
                dragStartScale = tc.Scale;
            }
            else {
                uint32_t id = pipeline.ReadPickingID((uint32_t)mx, (uint32_t)my);
                selectedID = id;
                pipeline.SetSelectedID(selectedID);
                selectedEntity = (selectedID == 0) ? Entity{} : scene.FindEntityByPickID(selectedID);
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            dragging = false;
        }

        // Apply gizmo while dragging
        if (dragging && gizmo != GizmoMode::None && selectedEntity) {
            float dx = mx - dragStartX;
            float dy = my - dragStartY;

            auto& tc = selectedEntity.GetComponent<TransformComponent>();

            if (gizmo == GizmoMode::Translate) {
                glm::vec3 right = editorCam.GetRight(); right.y = 0.f;
                glm::vec3 fwd = editorCam.GetForward(); fwd.y = 0.f;

                float rl = glm::length(right);
                float fl = glm::length(fwd);
                if (rl > 0.0001f) right /= rl; else right = { 1,0,0 };
                if (fl > 0.0001f) fwd /= fl; else fwd = { 0,0,-1 };

                float dist = glm::length(dragStartTranslation - editorCam.GetPosition());
                if (dist < 1.0f) dist = 1.0f;

                float scale = 0.0020f * dist;
                glm::vec3 delta = (right * dx + fwd * (-dy)) * scale;

                glm::vec3 out = dragStartTranslation + delta;
                if (ctrlDown) {
                    const float step = 0.5f;
                    out.x = SnapFloat(out.x, step);
                    out.z = SnapFloat(out.z, step);
                }
                tc.Translation = out;
            }
            else if (gizmo == GizmoMode::Rotate) {
                float speed = 0.01f;
                float yaw = dragStartRotation.y + dx * speed;
                if (ctrlDown) {
                    float step = 3.1415926f / 12.0f; // 15 deg
                    yaw = SnapFloat(yaw, step);
                }
                tc.Rotation.y = yaw;
            }
            else if (gizmo == GizmoMode::Scale) {
                float speed = 0.01f;
                float s = 1.0f + (-dy) * speed;
                if (s < 0.05f) s = 0.05f;
                glm::vec3 out = dragStartScale * s;

                if (ctrlDown) {
                    const float step = 0.1f;
                    out.x = SnapFloat(out.x, step);
                    out.y = SnapFloat(out.y, step);
                    out.z = SnapFloat(out.z, step);
                }
                tc.Scale = out;
            }
        }

        ImGui::End(); // Viewport window

        // --- End ImGui frame ---
        ImGui::Render();

        int displayW, displayH;
        glfwGetFramebufferSize(native, &displayW, &displayH);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.07f, 0.07f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(native);

        // Hotkeys (global)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) serializer.Serialize(scenePath);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
            serializer.Deserialize(scenePath);
            selectedID = 0;
            selectedEntity = {};
            pipeline.SetSelectedID(0);
            gizmo = GizmoMode::None;
            dragging = false;
        }
    }

    // --- ImGui shutdown ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}