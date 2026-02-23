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

static const char* AxisName(AxisConstraint a) {
    switch (a) {
    case AxisConstraint::X: return "X";
    case AxisConstraint::Y: return "Y";
    case AxisConstraint::Z: return "Z";
    default: return "None";
    }
}

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

struct GizmoMesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei Count = 0;
    GLenum Mode = GL_TRIANGLES;
    bool Indexed = true;
};

static GizmoMesh MakeIndexedMesh(const std::vector<glm::vec3>& verts,
    const std::vector<uint32_t>& idx,
    GLenum mode)
{
    GizmoMesh m;
    m.Mode = mode;
    m.Indexed = true;
    m.Count = (GLsizei)idx.size();

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glGenBuffers(1, &m.EBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
    return m;
}

static GizmoMesh MakeNonIndexedMesh(const std::vector<glm::vec3>& verts, GLenum mode)
{
    GizmoMesh m;
    m.Mode = mode;
    m.Indexed = false;
    m.Count = (GLsizei)verts.size();

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);

    glBindVertexArray(m.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
    return m;
}

static void DrawMesh(const GizmoMesh& m) {
    glBindVertexArray(m.VAO);
    if (m.Indexed) glDrawElements(m.Mode, m.Count, GL_UNSIGNED_INT, nullptr);
    else glDrawArrays(m.Mode, 0, m.Count);
    glBindVertexArray(0);
}

// Geometry builders
static GizmoMesh BuildUnitLineX() {
    std::vector<glm::vec3> v = { {0,0,0}, {1,0,0} };
    std::vector<uint32_t> i = { 0, 1 };
    return MakeIndexedMesh(v, i, GL_LINES);
}

static GizmoMesh BuildConeZ(int segments = 16) {
    // Tip at (0,0,1), base circle at z=0.75 radius=0.06
    std::vector<glm::vec3> v;
    std::vector<uint32_t> idx;

    const float baseZ = 0.75f;
    const float r = 0.06f;

    uint32_t tip = 0;
    v.push_back({ 0,0,1 });

    // base ring
    for (int s = 0; s < segments; s++) {
        float a = (float)s / (float)segments * 6.2831853f;
        v.push_back({ std::cos(a) * r, std::sin(a) * r, baseZ });
    }

    // side triangles
    for (int s = 0; s < segments; s++) {
        uint32_t a = 1 + s;
        uint32_t b = 1 + ((s + 1) % segments);
        idx.push_back(tip);
        idx.push_back(a);
        idx.push_back(b);
    }

    return MakeIndexedMesh(v, idx, GL_TRIANGLES);
}

static GizmoMesh BuildCube(float half = 0.06f) {
    // Cube centered at origin
    std::vector<glm::vec3> v = {
        {-half,-half,-half}, { half,-half,-half}, { half, half,-half}, {-half, half,-half},
        {-half,-half, half}, { half,-half, half}, { half, half, half}, {-half, half, half},
    };
    std::vector<uint32_t> i = {
        0,1,2, 2,3,0, // back
        4,5,6, 6,7,4, // front
        0,4,7, 7,3,0, // left
        1,5,6, 6,2,1, // right
        3,2,6, 6,7,3, // top
        0,1,5, 5,4,0  // bottom
    };
    return MakeIndexedMesh(v, i, GL_TRIANGLES);
}

static GizmoMesh BuildCircleXZ(int segments = 64) {
    std::vector<glm::vec3> v;
    v.reserve(segments);
    for (int s = 0; s < segments; s++) {
        float a = (float)s / (float)segments * 6.2831853f;
        v.push_back({ std::cos(a), 0.0f, std::sin(a) });
    }
    return MakeNonIndexedMesh(v, GL_LINE_LOOP);
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

    auto gizmoShader = std::make_shared<Shader>("Assets/Shaders/Gizmo.shader");

    GizmoMesh lineX = BuildUnitLineX();
    GizmoMesh coneZ = BuildConeZ();
    GizmoMesh cube = BuildCube();
    GizmoMesh ring = BuildCircleXZ();

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

        // 1) Define the viewport *rect* once (this is stable)
        ImVec2 vpMin = ImGui::GetCursorScreenPos();
        ImVec2 vpSize = ImGui::GetContentRegionAvail();
        if (vpSize.x < 1) vpSize.x = 1;
        if (vpSize.y < 1) vpSize.y = 1;
        ImVec2 vpMax = ImVec2(vpMin.x + vpSize.x, vpMin.y + vpSize.y);

        uint32_t vw = (uint32_t)vpSize.x;
        uint32_t vh = (uint32_t)vpSize.y;

        // 2) Hotkeys only when viewport focused
        bool viewportFocused = ImGui::IsWindowFocused();
        if (viewportFocused && !io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_G) && selectedEntity) gizmo = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_R) && selectedEntity) gizmo = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_F) && selectedEntity) gizmo = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gizmo = GizmoMode::None; dragging = false; }

            if (ImGui::IsKeyPressed(ImGuiKey_X)) axis = (axis == AxisConstraint::X) ? AxisConstraint::None : AxisConstraint::X;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) axis = (axis == AxisConstraint::Y) ? AxisConstraint::None : AxisConstraint::Y;
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) axis = (axis == AxisConstraint::Z) ? AxisConstraint::None : AxisConstraint::Z;
        }

        // 3) Camera control based on viewport RECT (not imageHovered)
        bool viewportRectHovered = ImGui::IsMouseHoveringRect(vpMin, vpMax, false);
        bool cameraControl = viewportRectHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);
        glfwSetInputMode(native, GLFW_CURSOR,
            cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // 4) Render passes (use updated camera)
        pipeline.BeginPickingPass(vw, vh, editorCam.GetCamera());
        scene.OnRenderPicking(editorCam.GetCamera(), pipeline.GetIDMaterial());
        pipeline.EndPickingPass();

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

        // Draw gizmos into scene FB here if you want (your existing code is fine)

        pipeline.Compose();

        // 5) Show composite texture
        ImTextureID tex = (ImTextureID)(intptr_t)pipeline.GetCompositeTexture();
        ImGui::Image(tex, vpSize, ImVec2(0, 1), ImVec2(1, 0));

        // 6) Now (and ONLY now) compute image hover + mouse coords relative to image
        bool imageHovered = ImGui::IsItemHovered();
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 imgMax = ImGui::GetItemRectMax();
        ImVec2 imgSize = ImVec2(imgMax.x - imgMin.x, imgMax.y - imgMin.y);

        ImVec2 mp = ImGui::GetMousePos();
        float mx = mp.x - imgMin.x;
        float my = mp.y - imgMin.y;

        // clamp
        if (mx < 0) mx = 0; if (my < 0) my = 0;
        if (mx > imgSize.x - 1) mx = imgSize.x - 1;
        if (my > imgSize.y - 1) my = imgSize.y - 1;

        bool ctrlDown = io.KeyCtrl;

        // 7) Click selection / start drag (don’t start if cameraControl is active)
        if (!cameraControl && imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
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

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            dragging = false;

        // Dragging applies transform with axis constraints
        if (!cameraControl && dragging && gizmo != GizmoMode::None && selectedEntity) {
            float dx = mx - dragStartX;
            float dy = my - dragStartY;

            auto& tc = selectedEntity.GetComponent<TransformComponent>();

            glm::vec3 camRight = editorCam.GetRight();
            glm::vec3 camFwd = editorCam.GetForward();
            glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFwd));

            float dist = glm::length(dragStartTranslation - editorCam.GetPosition());
            if (dist < 1.0f) dist = 1.0f;

            if (gizmo == GizmoMode::Translate) {
                glm::vec3 camRight = editorCam.GetRight();
                glm::vec3 camFwd = editorCam.GetForward();
                glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFwd));

                float dist = glm::length(dragStartTranslation - editorCam.GetPosition());
                if (dist < 1.0f) dist = 1.0f;
                float scale = 0.0020f * dist;

                glm::vec3 out = dragStartTranslation;

                if (axis == AxisConstraint::None) {
                    // existing XZ plane feel (camera-relative)
                    glm::vec3 right = camRight; right.y = 0.f;
                    glm::vec3 fwd = camFwd;   fwd.y = 0.f;
                    float rl = glm::length(right); if (rl > 0.0001f) right /= rl;
                    float fl = glm::length(fwd);   if (fl > 0.0001f) fwd /= fl;

                    glm::vec3 delta = (right * dx + fwd * (-dy)) * scale;
                    out = dragStartTranslation + delta;

                    if (ctrlDown) {
                        const float step = 0.5f;
                        out.x = SnapFloat(out.x, step);
                        out.z = SnapFloat(out.z, step);
                    }
                }
                else {
                    glm::vec3 A = (axis == AxisConstraint::X) ? glm::vec3(1, 0, 0)
                        : (axis == AxisConstraint::Y) ? glm::vec3(0, 1, 0)
                        : glm::vec3(0, 0, 1);

                    // project mouse motion onto axis using camera basis
                    glm::vec3 dragVec = camRight * dx + camUp * (-dy);
                    float amt = glm::dot(dragVec, A) * scale;
                    out = dragStartTranslation + A * amt;

                    if (ctrlDown) {
                        const float step = 0.5f;
                        out.x = SnapFloat(out.x, step);
                        out.y = SnapFloat(out.y, step);
                        out.z = SnapFloat(out.z, step);
                    }
                }

                tc.Translation = out;
            }
            else if (gizmo == GizmoMode::Rotate) {
                float speed = 0.01f;
                float delta = dx * speed;

                auto out = dragStartRotation;

                AxisConstraint ax = (axis == AxisConstraint::None) ? AxisConstraint::Y : axis;

                if (ax == AxisConstraint::X) out.x = dragStartRotation.x + delta;
                if (ax == AxisConstraint::Y) out.y = dragStartRotation.y + delta;
                if (ax == AxisConstraint::Z) out.z = dragStartRotation.z + delta;

                if (ctrlDown) {
                    float step = 3.1415926f / 12.0f; // 15 deg
                    out.x = SnapFloat(out.x, step);
                    out.y = SnapFloat(out.y, step);
                    out.z = SnapFloat(out.z, step);
                }

                tc.Rotation = out;
            }
            else if (gizmo == GizmoMode::Scale) {
                float speed = 0.01f;
                float s = 1.0f + (-dy) * speed;
                if (s < 0.05f) s = 0.05f;

                glm::vec3 out = dragStartScale;

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
                    out.x = SnapFloat(out.x, step);
                    out.y = SnapFloat(out.y, step);
                    out.z = SnapFloat(out.z, step);
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