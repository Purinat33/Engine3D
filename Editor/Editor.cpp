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

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstddef>

using namespace Engine;

enum class GizmoMode { None, Translate, Rotate, Scale };
enum class AxisConstraint { None, X, Y, Z };

static glm::vec3 AxisToVec(AxisConstraint a) {
    switch (a) {
    case AxisConstraint::X: return { 1,0,0 };
    case AxisConstraint::Y: return { 0,1,0 };
    case AxisConstraint::Z: return { 0,0,1 };
    default: return { 0,0,0 };
    }
}

static constexpr float kTranslateSense = 0.0020f;
static constexpr float kRotateSense = 0.0100f;
static constexpr float kScaleSense = 0.0100f;

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

// ---------------- Gizmo line renderer (Editor-only) ----------------

struct GizmoVertex {
    glm::vec3 Pos;
    glm::vec3 Col;
};

struct GizmoRenderer {
    GLuint VAO = 0, VBO = 0;
    std::shared_ptr<Shader> GizmoShader;
    bool Initialized = false;

    void Init() {
        if (Initialized) return;

        // Needs file: Assets/Shaders/GizmoLine.shader
        GizmoShader = std::make_shared<Shader>("Assets/Shaders/GizmoLine.shader");

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // enough for a bunch of lines
        glBufferData(GL_ARRAY_BUFFER, 1024 * 1024, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (const void*)offsetof(GizmoVertex, Pos));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (const void*)offsetof(GizmoVertex, Col));

        glBindVertexArray(0);
        Initialized = true;
    }

    void Draw(const PerspectiveCamera& cam, const std::vector<GizmoVertex>& verts, float opacity = 1.0f) {
        if (!Initialized || verts.empty()) return;

        GizmoShader->Bind();
        GizmoShader->SetMat4("u_ViewProjection", glm::value_ptr(cam.GetViewProjection()));
        GizmoShader->SetFloat("u_Opacity", opacity);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(verts.size() * sizeof(GizmoVertex)), verts.data());

        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)verts.size());

        glBindVertexArray(0);
    }
};

static void AddLine(std::vector<GizmoVertex>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    out.push_back({ a, c });
    out.push_back({ b, c });
}

static void AddArrow(std::vector<GizmoVertex>& out, const glm::vec3& base, const glm::vec3& dir,
    const glm::vec3& color, float len, float headLen, float headWidth) {

    glm::vec3 d = glm::normalize(dir);
    glm::vec3 end = base + d * len;
    AddLine(out, base, end, color);

    glm::vec3 up = (std::abs(d.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(d, up));

    glm::vec3 headA = end - d * headLen + right * headWidth;
    glm::vec3 headB = end - d * headLen - right * headWidth;

    AddLine(out, end, headA, color);
    AddLine(out, end, headB, color);
}

static void AddBox(std::vector<GizmoVertex>& out, const glm::vec3& center, const glm::vec3& u, const glm::vec3& v,
    const glm::vec3& color, float size) {

    glm::vec3 uu = glm::normalize(u);
    glm::vec3 vv = glm::normalize(v);

    glm::vec3 a = center + uu * size + vv * size;
    glm::vec3 b = center - uu * size + vv * size;
    glm::vec3 c = center - uu * size - vv * size;
    glm::vec3 d = center + uu * size - vv * size;

    AddLine(out, a, b, color);
    AddLine(out, b, c, color);
    AddLine(out, c, d, color);
    AddLine(out, d, a, color);
}

static void AddCircle(std::vector<GizmoVertex>& out, const glm::vec3& center,
    const glm::vec3& axis, const glm::vec3& color, float radius, int segments = 64) {

    glm::vec3 n = glm::normalize(axis);
    glm::vec3 tmp = (std::abs(n.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 u = glm::normalize(glm::cross(n, tmp));
    glm::vec3 v = glm::normalize(glm::cross(n, u));

    for (int i = 0; i < segments; i++) {
        float a0 = (float)i * (2.0f * 3.1415926f / segments);
        float a1 = (float)(i + 1) * (2.0f * 3.1415926f / segments);

        glm::vec3 p0 = center + (u * std::cos(a0) + v * std::sin(a0)) * radius;
        glm::vec3 p1 = center + (u * std::cos(a1) + v * std::sin(a1)) * radius;

        AddLine(out, p0, p1, color);
    }
}

// ---------------- Main ----------------

int main() {
    auto window = Window::Create({ "Engine3D Editor (ImGui)", 1600, 900 });
    GLFWwindow* native = (GLFWwindow*)window->GetNativeWindow();

    Renderer::Init();

    // Axis constraint + gizmo renderer (needs GL ready -> after Renderer::Init())
    AxisConstraint axis = AxisConstraint::None;
    GizmoRenderer gizmoRenderer;
    gizmoRenderer.Init();

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
    bool viewportFocused = false;

    auto last = std::chrono::high_resolution_clock::now();

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
        {
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

            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Save", "Ctrl+S")) serializer.Serialize(scenePath);
                    if (ImGui::MenuItem("Reload", "Ctrl+R")) {
                        serializer.Deserialize(scenePath);
                        selectedID = 0;
                        selectedEntity = {};
                        pipeline.SetSelectedID(0);
                        gizmo = GizmoMode::None;
                        dragging = false;
                        axis = AxisConstraint::None;
                    }
                    if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(native, 1);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::End();
        }

        // --- Hierarchy ---
        {
            ImGui::Begin("Hierarchy");
            auto view = scene.Registry().view<IDComponent, TagComponent>();
            view.each([&](auto, IDComponent& idc, TagComponent& tc) {
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
                axis = AxisConstraint::None;
            }
            ImGui::End();
        }

        // --- Inspector ---
        {
            ImGui::Begin("Inspector");
            if (selectedEntity) {
                auto& tag = selectedEntity.GetComponent<TagComponent>().Tag;
                ImGui::Text("Entity: %s", tag.c_str());
                ImGui::Text("PickID: %u", selectedID);
                ImGui::Text("Axis: %s",
                    axis == AxisConstraint::X ? "X" :
                    axis == AxisConstraint::Y ? "Y" :
                    axis == AxisConstraint::Z ? "Z" : "None");

                auto& tr = selectedEntity.GetComponent<TransformComponent>();

                ImGui::Separator();
                ImGui::Text("Transform");
                ImGui::DragFloat3("Translation", &tr.Translation.x, 0.05f);

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
                ImGui::Text("Hold Ctrl = snap. X/Y/Z = axis constraint.");
            }
            else {
                ImGui::Text("No selection.");
            }
            ImGui::End();
        }

        // --- Viewport ---
        ImGui::Begin("Viewport");
        viewportPos = ImGui::GetCursorScreenPos();
        viewportSize = ImGui::GetContentRegionAvail();
        if (viewportSize.x < 1) viewportSize.x = 1;
        if (viewportSize.y < 1) viewportSize.y = 1;

        uint32_t vw = (uint32_t)viewportSize.x;
        uint32_t vh = (uint32_t)viewportSize.y;

        // Focus state for hotkeys
        viewportFocused = ImGui::IsWindowFocused();

        // Hotkeys (only when viewport focused and not typing in widgets)
        if (viewportFocused && !io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_G) && selectedEntity) gizmo = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_R) && selectedEntity) gizmo = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_F) && selectedEntity) gizmo = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gizmo = GizmoMode::None; dragging = false; }

            if (ImGui::IsKeyPressed(ImGuiKey_X)) axis = (axis == AxisConstraint::X) ? AxisConstraint::None : AxisConstraint::X;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) axis = (axis == AxisConstraint::Y) ? AxisConstraint::None : AxisConstraint::Y;
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) axis = (axis == AxisConstraint::Z) ? AxisConstraint::None : AxisConstraint::Z;
        }

        // Camera control: RMB held anywhere in viewport window
        bool cameraControl = ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right);
        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);
        glfwSetInputMode(native, GLFW_CURSOR, cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // --- Picking pass ---
        pipeline.BeginPickingPass(vw, vh, editorCam.GetCamera());
        scene.OnRenderPicking(editorCam.GetCamera(), pipeline.GetIDMaterial());
        pipeline.EndPickingPass();

        // --- Scene pass ---
        pipeline.BeginScenePass(vw, vh, editorCam.GetCamera());

        // Grid
        gridShader->Bind();
        gridShader->SetFloat("u_GridScale", 1.0f);
        gridShader->SetFloat3("u_GridColor", 0.45f, 0.45f, 0.45f);
        gridShader->SetFloat3("u_BaseColor", 0.12f, 0.12f, 0.12f);
        gridShader->SetFloat("u_Opacity", 0.30f);
        Renderer::Submit(gridMat, gridVAO, glm::mat4(1.0f));

        scene.OnUpdate(dt);
        scene.OnRender(editorCam.GetCamera());

        pipeline.EndScenePass();

        // --- Draw gizmo visuals into the scene framebuffer (scene FB still bound here) ---
        if (selectedEntity && gizmo != GizmoMode::None) {
            std::vector<GizmoVertex> verts;
            verts.reserve(4096);

            glm::vec3 p = selectedEntity.GetComponent<TransformComponent>().Translation;

            float dist = glm::length(p - editorCam.GetPosition());
            if (dist < 1.0f) dist = 1.0f;

            // scale gizmo to keep it readable
            float g = std::max(0.8f, dist * 0.15f);

            glm::vec3 colX = (axis == AxisConstraint::X) ? glm::vec3(1, 1, 0) : glm::vec3(1, 0.2f, 0.2f);
            glm::vec3 colY = (axis == AxisConstraint::Y) ? glm::vec3(1, 1, 0) : glm::vec3(0.2f, 1, 0.2f);
            glm::vec3 colZ = (axis == AxisConstraint::Z) ? glm::vec3(1, 1, 0) : glm::vec3(0.2f, 0.6f, 1);

            if (gizmo == GizmoMode::Translate) {
                AddArrow(verts, p, { 1,0,0 }, colX, g, g * 0.18f, g * 0.07f);
                AddArrow(verts, p, { 0,1,0 }, colY, g, g * 0.18f, g * 0.07f);
                AddArrow(verts, p, { 0,0,1 }, colZ, g, g * 0.18f, g * 0.07f);
            }
            else if (gizmo == GizmoMode::Rotate) {
                AddCircle(verts, p, { 1,0,0 }, colX, g * 0.85f, 64);
                AddCircle(verts, p, { 0,1,0 }, colY, g * 0.85f, 64);
                AddCircle(verts, p, { 0,0,1 }, colZ, g * 0.85f, 64);
            }
            else if (gizmo == GizmoMode::Scale) {
                AddLine(verts, p, p + glm::vec3(g, 0, 0), colX);
                AddLine(verts, p, p + glm::vec3(0, g, 0), colY);
                AddLine(verts, p, p + glm::vec3(0, 0, g), colZ);

                AddBox(verts, p + glm::vec3(g, 0, 0), { 0,1,0 }, { 0,0,1 }, colX, g * 0.05f);
                AddBox(verts, p + glm::vec3(0, g, 0), { 1,0,0 }, { 0,0,1 }, colY, g * 0.05f);
                AddBox(verts, p + glm::vec3(0, 0, g), { 1,0,0 }, { 0,1,0 }, colZ, g * 0.05f);
            }

            // draw on top
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            gizmoRenderer.Draw(editorCam.GetCamera(), verts, 1.0f);

            glEnable(GL_DEPTH_TEST);
        }

        // Compose scene + selection outline
        pipeline.Compose();

        // Show composite in ImGui
        ImTextureID tex = (ImTextureID)(intptr_t)pipeline.GetCompositeTexture();
        ImGui::Image(tex, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        bool imageHovered = ImGui::IsItemHovered();

        // Mouse position relative to image
        ImVec2 mp = ImGui::GetMousePos();
        float mx = mp.x - viewportPos.x;
        float my = mp.y - viewportPos.y;

        bool ctrlDown = io.KeyCtrl;

        // Click selection / start drag
        if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse) {
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
                if (selectedID == 0) axis = AxisConstraint::None;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            dragging = false;
        }

        // Dragging applies transform with axis constraints
        if (dragging && gizmo != GizmoMode::None && selectedEntity) {
            float dx = mx - dragStartX;
            float dy = my - dragStartY;

            auto& tc = selectedEntity.GetComponent<TransformComponent>();

            glm::vec3 camRight = editorCam.GetRight();
            glm::vec3 camFwd = editorCam.GetForward();
            glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFwd));

            float dist = glm::length(dragStartTranslation - editorCam.GetPosition());
            if (dist < 1.0f) dist = 1.0f;

            if (gizmo == GizmoMode::Translate) {
                glm::vec3 out = dragStartTranslation;

                if (axis == AxisConstraint::None) {
                    // camera-relative XZ
                    glm::vec3 right = camRight; right.y = 0.f;
                    glm::vec3 fwd = camFwd;   fwd.y = 0.f;
                    if (glm::length(right) > 0.0001f) right = glm::normalize(right); else right = { 1,0,0 };
                    if (glm::length(fwd) > 0.0001f) fwd = glm::normalize(fwd);   else fwd = { 0,0,-1 };

                    float scale = kTranslateSense * dist;
                    glm::vec3 delta = (right * dx + fwd * (-dy)) * scale;
                    out = dragStartTranslation + delta;

                    if (ctrlDown) {
                        const float step = 0.5f;
                        out.x = SnapFloat(out.x, step);
                        out.z = SnapFloat(out.z, step);
                    }
                }
                else {
                    glm::vec3 a = AxisToVec(axis);
                    float scale = kTranslateSense * dist;

                    float amount = (dx * glm::dot(a, camRight) + (-dy) * glm::dot(a, camUp)) * scale;
                    out = dragStartTranslation + a * amount;

                    if (ctrlDown) {
                        const float step = 0.5f;
                        if (axis == AxisConstraint::X) out.x = SnapFloat(out.x, step);
                        if (axis == AxisConstraint::Y) out.y = SnapFloat(out.y, step);
                        if (axis == AxisConstraint::Z) out.z = SnapFloat(out.z, step);
                    }
                }

                tc.Translation = out;
            }
            else if (gizmo == GizmoMode::Rotate) {
                glm::vec3 out = dragStartRotation;

                AxisConstraint useAxis = (axis == AxisConstraint::None) ? AxisConstraint::Y : axis;
                float angle = dx * kRotateSense;

                if (ctrlDown) {
                    float step = 3.1415926f / 12.0f; // 15 deg
                    angle = SnapFloat(angle, step);
                }

                if (useAxis == AxisConstraint::X) out.x = dragStartRotation.x + angle;
                if (useAxis == AxisConstraint::Y) out.y = dragStartRotation.y + angle;
                if (useAxis == AxisConstraint::Z) out.z = dragStartRotation.z + angle;

                tc.Rotation = out;
            }
            else if (gizmo == GizmoMode::Scale) {
                glm::vec3 out = dragStartScale;

                float s = 1.0f + (-dy) * kScaleSense;
                if (s < 0.05f) s = 0.05f;

                if (axis == AxisConstraint::None) {
                    out = dragStartScale * s;
                }
                else {
                    if (axis == AxisConstraint::X) out.x = dragStartScale.x * s;
                    if (axis == AxisConstraint::Y) out.y = dragStartScale.y * s;
                    if (axis == AxisConstraint::Z) out.z = dragStartScale.z * s;
                }

                if (ctrlDown) {
                    const float step = 0.1f;
                    if (axis == AxisConstraint::None) {
                        out.x = SnapFloat(out.x, step);
                        out.y = SnapFloat(out.y, step);
                        out.z = SnapFloat(out.z, step);
                    }
                    else {
                        if (axis == AxisConstraint::X) out.x = SnapFloat(out.x, step);
                        if (axis == AxisConstraint::Y) out.y = SnapFloat(out.y, step);
                        if (axis == AxisConstraint::Z) out.z = SnapFloat(out.z, step);
                    }
                }

                tc.Scale = out;
            }
        }

        ImGui::End(); // Viewport

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

        // Global hotkeys
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) serializer.Serialize(scenePath);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
            serializer.Deserialize(scenePath);
            selectedID = 0;
            selectedEntity = {};
            pipeline.SetSelectedID(0);
            gizmo = GizmoMode::None;
            dragging = false;
            axis = AxisConstraint::None;
        }
    }

    // --- ImGui shutdown ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}