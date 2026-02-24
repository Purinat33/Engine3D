#include <Engine/Core/Window.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/RendererPipeline.h>
#include <Engine/Renderer/CameraController.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Scene/UUID.h>

#include <Engine/Assets/AssetManager.h>

#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/VertexArray.h>
#include <Engine/Renderer/Buffer.h>

#include "CommandStack.h"
#include "EditorSceneManager.h"
#include "ProjectSettings.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Engine/Core/Content.h>
#include <Engine/Renderer/Texture2D.h>
#include <Engine/Renderer/Model.h>

#include "Engine/Renderer/TextureCube.h"

#include <filesystem>
#include <algorithm>

#include <chrono>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstddef>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

using namespace Engine;

enum class GizmoMode { None, Translate, Rotate, Scale };
enum class AxisConstraint { None, X, Y, Z };

static bool openNewScenePopup = false;
static char newSceneNameBuf[128] = "NewScene";
static std::string statusText;
static float statusTimer = 0.0f;


static const char* AxisName(AxisConstraint a) {
    switch (a) {
    case AxisConstraint::X: return "X";
    case AxisConstraint::Y: return "Y";
    case AxisConstraint::Z: return "Z";
    default: return "None";
    }
}

static float SnapFloat(float v, float step) {
    if (step <= 0.0f) return v;
    return std::round(v / step) * step;
}

static uint32_t FoldUUIDToPickID(UUID id) {
    uint32_t v = (uint32_t)(id ^ (id >> 32));
    return v == 0 ? 1u : v;
}

// Icons
static std::shared_ptr<VertexArray> CreateIconQuadVAO() {
    // pos.xyz, uv.xy (centered quad in XY plane)
    float v[] = {
        -0.5f, -0.5f, 0.0f,  0.f, 0.f,
         0.5f, -0.5f, 0.0f,  1.f, 0.f,
         0.5f,  0.5f, 0.0f,  1.f, 1.f,
        -0.5f,  0.5f, 0.0f,  0.f, 1.f
    };
    uint32_t idx[] = { 0, 1, 2, 2, 3, 0 };

    auto vao = std::make_shared<VertexArray>();
    auto vb = std::make_shared<VertexBuffer>(v, sizeof(v));
    vb->SetLayout({
        { ShaderDataType::Float3 }, // pos
        { ShaderDataType::Float2 }  // uv
        });
    vao->AddVertexBuffer(vb);

    auto ib = std::make_shared<IndexBuffer>(idx, 6);
    vao->SetIndexBuffer(ib);
    return vao;
}

// ---------------- Grid ----------------
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

// ---------------- Gizmo Lines (Editor-only) ----------------
struct GizmoVertex { glm::vec3 Pos; glm::vec3 Col; };

struct GizmoRenderer {
    GLuint VAO = 0, VBO = 0;
    std::shared_ptr<Shader> ShaderLine;
    bool Initialized = false;

    void Init() {
        if (Initialized) return;
        ShaderLine = std::make_shared<Shader>("Assets/Shaders/GizmoLine.shader");

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
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

        ShaderLine->Bind();
        ShaderLine->SetMat4("u_ViewProjection", glm::value_ptr(cam.GetViewProjection()));
        ShaderLine->SetFloat("u_Opacity", opacity);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(verts.size() * sizeof(GizmoVertex)), verts.data());

        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)verts.size());

        glBindVertexArray(0);
    }
};

static void AddLine(std::vector<GizmoVertex>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    out.push_back({ a, c }); out.push_back({ b, c });
}

static void AddArrow(std::vector<GizmoVertex>& out,
    const glm::vec3& base, const glm::vec3& dir,
    const glm::vec3& color,
    float len, float headLen, float headWidth)
{
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

static void AddBox(std::vector<GizmoVertex>& out,
    const glm::vec3& center, const glm::vec3& u, const glm::vec3& v,
    const glm::vec3& color, float size)
{
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

static void AddCircle(std::vector<GizmoVertex>& out,
    const glm::vec3& center, const glm::vec3& axis,
    const glm::vec3& color, float radius, int segments = 64)
{
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

static std::string MakeNameFromPath(const std::string& path) {
    std::filesystem::path p(path);
    if (p.has_stem()) return p.stem().string();
    return "Model";
}

static void BuildAssetLists(std::vector<std::pair<AssetHandle, std::string>>& outModels,
    std::vector<std::pair<AssetHandle, std::string>>& outShaders)
{
    outModels.clear();
    outShaders.clear();

    auto& assets = AssetManager::Get();
    const auto& all = assets.Registry().GetAll();

    for (const auto& [id, meta] : all) {
        if (meta.Type == AssetType::Model) outModels.emplace_back(id, meta.Path);
        if (meta.Type == AssetType::Shader) outShaders.emplace_back(id, meta.Path);
    }

    auto byPath = [](auto& a, auto& b) { return a.second < b.second; };
    std::sort(outModels.begin(), outModels.end(), byPath);
    std::sort(outShaders.begin(), outShaders.end(), byPath);
}

static void UpdateWindowTitle(GLFWwindow* native, const std::string& sceneName, bool dirty) {
    std::string title = "Engine3D Editor - " + sceneName;
    if (dirty) title += " *";
    glfwSetWindowTitle(native, title.c_str());
}

static void ScanModelsOnDisk(const std::string& root, std::vector<std::string>& out) {
    out.clear();
    namespace fs = std::filesystem;

    if (!fs::exists(root)) return;

    for (auto& it : fs::recursive_directory_iterator(root)) {
        if (!it.is_regular_file()) continue;

        auto ext = it.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
            out.push_back(it.path().generic_string());
        }
    }
    std::sort(out.begin(), out.end());
}

// Helper for Editor Models
static glm::vec3 EulerFromForward(const glm::vec3& fwd) {
    glm::vec3 f = glm::normalize(fwd);
    float yaw = std::atan2(f.x, -f.z);                     // -Z forward baseline
    float pitch = std::asin(glm::clamp(f.y, -1.0f, 1.0f));
    return glm::vec3(pitch, yaw, 0.0f);
}

//glm::mat4 markerAxisFix = glm::rotate(glm::mat4(1.0f),
//    glm::radians(90.0f),
//    glm::vec3(1, 0, 0)); // +90° around X
//
//glm::mat4 markerFlipZ =
//glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0)); // +Z -> -Z
static const glm::mat4 markerFix =
glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0)); // +Y -> -Z


int main() {
    auto window = Window::Create({ "Engine3D Editor", 1600, 900 });
    GLFWwindow* native = (GLFWwindow*)window->GetNativeWindow();

    Renderer::Init();

    // Skybox
    Renderer::SetSkybox(std::make_shared<TextureCube>(std::array<std::string, 6>{
        "Assets/Skybox/px.png",
            "Assets/Skybox/nx.png",
            "Assets/Skybox/py.png",
            "Assets/Skybox/ny.png",
            "Assets/Skybox/pz.png",
            "Assets/Skybox/nz.png"
    }));

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(native, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Scene + serializer
    Scene scene;

    // Pipeline + camera
    RendererPipeline pipeline;
    CameraController editorCam(1.0472f, 1600.0f / 900.0f, 0.1f, 300.0f);
    editorCam.SetTransform(glm::vec3(0.0f, 8.0f, 8.0f), -3.1415926f, -0.75f);

    // Grid
    auto gridShader = std::make_shared<Shader>("Assets/Shaders/Grid.shader");
    auto gridMat = std::make_shared<Material>(gridShader);
    gridMat->SetTwoSided(true);
    auto gridVAO = CreateGridPlaneVAO(100.0f);

    // Gizmo renderer
    GizmoRenderer gizmoRenderer;
    gizmoRenderer.Init();

    // Models for Editors
    // --- Editor Marker Models (3D) ---
    std::shared_ptr<Shader> markerShader;
    std::shared_ptr<Model> markerLight;
    std::shared_ptr<Model> markerSpawn;
    std::shared_ptr<Model> markerWarp;

    try {
        // Use whatever shader you normally use for meshes.
        // IMPORTANT: this must exist on disk.
        markerShader = std::make_shared<Shader>(Engine::Content::Resolve("Shaders/Marker.shader"));

        markerLight = std::make_shared<Model>(Engine::Content::Resolve("Editor/Markers/light.glb"), markerShader);
        markerSpawn = std::make_shared<Model>(Engine::Content::Resolve("Editor/Markers/spawn.glb"), markerShader);
        markerWarp = std::make_shared<Model>(Engine::Content::Resolve("Editor/Markers/warp.glb"), markerShader);
        
        auto SetupMarkerModel = [&](const std::shared_ptr<Model>& m, const glm::vec4& color) {
            if (!m) return;
            for (const auto& sm : m->GetSubMeshes()) {
                if (sm.MaterialPtr) {
                    sm.MaterialPtr->SetColor(color);
                    sm.MaterialPtr->SetTwoSided(true);
                }
            }
            };

        SetupMarkerModel(markerLight, { 1.f, 1.f, 0.2f, 1.f }); // yellow-ish
        SetupMarkerModel(markerSpawn, { 0.2f, 1.f, 0.2f, 1.f }); // green-ish
        SetupMarkerModel(markerWarp, { 0.2f, 0.6f, 1.f, 1.f }); // blue-ish
    
    }
    catch (const std::exception& e) {
        std::cout << "[Editor] Marker models disabled: " << e.what() << "\n";
        markerShader.reset();
        markerLight.reset();
        markerSpawn.reset();
        markerWarp.reset();
    }

    // -------- Undo/Selection State --------
    EditorUndo::CommandStack cmdStack;
    UUID selectedUUID = 0;
    uint32_t selectedPickID = 0;
    Entity selectedEntity{};

    GizmoMode gizmo = GizmoMode::None;
    AxisConstraint axis = AxisConstraint::None;

    bool dragging = false;
    float dragStartX = 0.f, dragStartY = 0.f;
    glm::vec3 dragStartTranslation{ 0.f };
    glm::vec3 dragStartRotation{ 0.f };
    glm::vec3 dragStartScale{ 1.f };

    auto ClearSelection = [&]() {
        selectedUUID = 0;
        selectedPickID = 0;
        selectedEntity = {};
        pipeline.SetSelectedID(0);
        axis = AxisConstraint::None;
        gizmo = GizmoMode::None;
        dragging = false;
        };

    auto SyncSelection = [&]() {
        if (selectedUUID == 0) { ClearSelection(); return; }
        Entity e = scene.FindEntityByUUID(selectedUUID);
        if (!e) { ClearSelection(); return; }
        selectedEntity = e;
        selectedPickID = FoldUUIDToPickID(selectedUUID);
        pipeline.SetSelectedID(selectedPickID);
        };

    auto SelectByUUID = [&](UUID id) {
        selectedUUID = id;
        SyncSelection();
        };

    auto SelectByPickID = [&](uint32_t pid) {
        if (pid == 0) { ClearSelection(); return; }
        Entity e = scene.FindEntityByPickID(pid);
        if (!e) { ClearSelection(); return; }
        selectedUUID = e.GetComponent<IDComponent>().ID;
        SyncSelection();
        };

    // -------- Scene workflow manager --------
    EditorSceneManager sceneMgr(scene, cmdStack, [&]() {
        ClearSelection();
        });

    // Start by opening default if exists, else create new
    if (!sceneMgr.OpenScene("Assets/Scenes/Sandbox.scene")) {
        sceneMgr.NewScene();
        sceneMgr.SaveAs("Assets/Scenes/Sandbox.scene");
    }

    // Unsaved changes modal state
    enum class PendingAction { None, NewScene, OpenScene };
    PendingAction pending = PendingAction::None;
    std::string pendingOpenPath;

    // Save As popup buffer
    static char saveAsBuf[260] = "Assets/Scenes/NewScene.scene";

    // Inspector undo snapshot
    static EditorUndo::TransformSnapshot inspectorBefore{};

    // Timing
    auto last = std::chrono::high_resolution_clock::now();

    while (!window->ShouldClose()) {
        glfwPollEvents();

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        UpdateWindowTitle(native, sceneMgr.GetDisplayName(), sceneMgr.IsDirty());

        // Begin ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Dockspace
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

            // --- popup requests (IMPORTANT) ---
            static bool requestOpenSaveAs = false;

            // Menu
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {

                    if (ImGui::MenuItem("New...")) {
                        if (sceneMgr.IsDirty()) {
                            pending = PendingAction::NewScene;
                            ImGui::OpenPopup("Unsaved Changes");
                        }
                        else {
                            openNewScenePopup = true; // request
                        }
                    }

                    if (ImGui::BeginMenu("Open")) {
                        sceneMgr.RefreshSceneList();
                        for (const auto& s : sceneMgr.GetSceneList()) {
                            if (ImGui::MenuItem(s.c_str())) {
                                if (sceneMgr.IsDirty()) {
                                    pending = PendingAction::OpenScene;
                                    pendingOpenPath = s;
                                    ImGui::OpenPopup("Unsaved Changes");
                                }
                                else {
                                    sceneMgr.OpenScene(s);
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("Save", "Ctrl+S")) {
                        if (sceneMgr.Save()) {
                            statusText = "Saved.";
                            statusTimer = 2.0f;
                        }
                        else {
                            std::snprintf(saveAsBuf, sizeof(saveAsBuf), "%s", "Assets/Scenes/NewScene.scene");
                            requestOpenSaveAs = true; // request (DON'T OpenPopup here)
                            statusText = "Scene has no path. Use Save As.";
                            statusTimer = 3.0f;
                        }
                    }

                    if (ImGui::MenuItem("Save As...")) {
                        std::string guess = sceneMgr.GetCurrentPath().empty()
                            ? "Assets/Scenes/NewScene.scene"
                            : sceneMgr.GetCurrentPath();
                        std::snprintf(saveAsBuf, sizeof(saveAsBuf), "%s", guess.c_str());
                        requestOpenSaveAs = true; // request
                    }

                    if (ImGui::MenuItem("Set As Startup Scene")) {
                        if (!sceneMgr.GetCurrentPath().empty()) {
                            EditorProject::SaveStartupScene(sceneMgr.GetCurrentPath());
                        }
                    }

                    if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(native, 1);

                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            // --- Open popups OUTSIDE menus/modals ---
            if (openNewScenePopup) {
                ImGui::OpenPopup("New Scene");
                openNewScenePopup = false;
            }
            if (requestOpenSaveAs) {
                ImGui::OpenPopup("Save As");
                requestOpenSaveAs = false;
            }

            // --- Unsaved Changes Modal ---
            if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("You have unsaved changes.\nWhat do you want to do?");
                ImGui::Separator();

                if (ImGui::Button("Save")) {
                    if (!sceneMgr.Save()) {
                        // MUST close this modal before opening another modal
                        std::snprintf(saveAsBuf, sizeof(saveAsBuf), "%s", "Assets/Scenes/NewScene.scene");
                        ImGui::CloseCurrentPopup();
                        ImGui::OpenPopup("Save As");
                        // keep pending as-is; Save As will continue it
                    }
                    else {
                        ImGui::CloseCurrentPopup();

                        if (pending == PendingAction::NewScene) openNewScenePopup = true;
                        if (pending == PendingAction::OpenScene) sceneMgr.OpenScene(pendingOpenPath);

                        pending = PendingAction::None;
                        pendingOpenPath.clear();
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Don't Save")) {
                    ImGui::CloseCurrentPopup();

                    if (pending == PendingAction::NewScene) openNewScenePopup = true;
                    if (pending == PendingAction::OpenScene) sceneMgr.OpenScene(pendingOpenPath);

                    pending = PendingAction::None;
                    pendingOpenPath.clear();
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    pending = PendingAction::None;
                    pendingOpenPath.clear();
                }

                ImGui::EndPopup();
            }

            // --- New Scene Modal ---
            if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Create a new scene in Assets/Scenes/");
                ImGui::InputText("Name", newSceneNameBuf, sizeof(newSceneNameBuf));
                ImGui::Separator();

                std::string newPath = std::string("Assets/Scenes/") + newSceneNameBuf + ".scene";
                ImGui::Text("Path: %s", newPath.c_str());

                if (ImGui::Button("Create")) {
                    sceneMgr.NewScene();

                    if (sceneMgr.SaveAs(newPath)) {
                        statusText = "Created: " + newPath;
                        statusTimer = 3.0f;
                    }
                    else {
                        statusText = "Failed to create: " + newPath;
                        statusTimer = 5.0f;
                    }

                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // --- Save As Modal ---
            if (ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Save scene as:");
                ImGui::InputText("Path", saveAsBuf, sizeof(saveAsBuf));
                ImGui::Separator();

                if (ImGui::Button("Save")) {
                    if (sceneMgr.SaveAs(saveAsBuf)) {
                        statusText = std::string("Saved As: ") + saveAsBuf;
                        statusTimer = 3.0f;
                    }
                    else {
                        statusText = std::string("Save As failed: ") + saveAsBuf;
                        statusTimer = 5.0f;
                    }

                    ImGui::CloseCurrentPopup();

                    // Continue pending action after Save As
                    if (pending != PendingAction::None) {
                        if (pending == PendingAction::NewScene) openNewScenePopup = true;
                        if (pending == PendingAction::OpenScene) sceneMgr.OpenScene(pendingOpenPath);

                        pending = PendingAction::None;
                        pendingOpenPath.clear();
                    }
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            ImGui::End(); // DockSpace
        }

        // -------- Asset Browser --------
        static char importModelPathBuf[260] = "Assets/Models/monkey.obj";
        static int importShaderIndex = 0;

        static std::vector<std::pair<AssetHandle, std::string>> s_ModelAssets;
        static std::vector<std::pair<AssetHandle, std::string>> s_ShaderAssets;
        static bool s_AssetListsBuilt = false;

        if (!s_AssetListsBuilt) {
            BuildAssetLists(s_ModelAssets, s_ShaderAssets);
            s_AssetListsBuilt = true;
        }

        ImGui::Begin("Assets");

        // Refresh
        if (ImGui::Button("Refresh Assets")) {
            BuildAssetLists(s_ModelAssets, s_ShaderAssets);
        }
        ImGui::Separator();

        // Import Model
        ImGui::Text("Import Model");
        ImGui::InputText("Model Path", importModelPathBuf, sizeof(importModelPathBuf));
        // ---- Disk scan (put this in the Assets window) ----
        static std::vector<std::string> diskModels;
        static char diskFilter[128] = "";

        if (ImGui::Button("Scan Assets/Models")) {
            ScanModelsOnDisk("Assets/Models", diskModels);
        }
        ImGui::SameLine();
        ImGui::InputTextWithHint("##DiskFilter", "filter (e.g. wall)", diskFilter, sizeof(diskFilter));

        ImGui::BeginChild("DiskModelsList", ImVec2(0, 140), true);
        for (auto& p : diskModels) {
            if (diskFilter[0] != '\0') {
                std::string lowP = p, lowF = diskFilter;
                std::transform(lowP.begin(), lowP.end(), lowP.begin(), ::tolower);
                std::transform(lowF.begin(), lowF.end(), lowF.begin(), ::tolower);
                if (lowP.find(lowF) == std::string::npos) continue;
            }

            if (ImGui::Selectable(p.c_str())) {
                std::snprintf(importModelPathBuf, sizeof(importModelPathBuf), "%s", p.c_str());
            }
        }
        ImGui::EndChild();

        // Shader combo
        if (s_ShaderAssets.empty()) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No shaders registered. Import a shader first.");
        }
        else {
            if (importShaderIndex < 0) importShaderIndex = 0;
            if (importShaderIndex >= (int)s_ShaderAssets.size()) importShaderIndex = (int)s_ShaderAssets.size() - 1;

            const char* currentShaderLabel = s_ShaderAssets[importShaderIndex].second.c_str();
            if (ImGui::BeginCombo("Shader", currentShaderLabel)) {
                for (int i = 0; i < (int)s_ShaderAssets.size(); i++) {
                    bool selected = (i == importShaderIndex);
                    if (ImGui::Selectable(s_ShaderAssets[i].second.c_str(), selected)) {
                        importShaderIndex = i;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Import + Instantiate")) {
                std::string modelPath = importModelPathBuf;
                AssetHandle shaderH = s_ShaderAssets[importShaderIndex].first;

                auto& assets = AssetManager::Get();
                AssetHandle modelH = assets.LoadModel(modelPath, shaderH);

                // refresh lists (new asset)
                BuildAssetLists(s_ModelAssets, s_ShaderAssets);

                if (modelH != InvalidAssetHandle) {
                    std::string name = MakeNameFromPath(modelPath);
                    Entity e = scene.CreateEntity(name.c_str());
                    e.AddComponent<MeshRendererComponent>(modelH);
                    sceneMgr.MarkDirty();

                    // Select it
                    selectedUUID = e.GetComponent<IDComponent>().ID;
                    SyncSelection();
                }
            }
        }

        ImGui::Separator();

        // Models list
        ImGui::Text("Models");
        for (auto& [h, path] : s_ModelAssets) {
            // 64-bit safe PushID
            ImGui::PushID((void*)(uintptr_t)h);

            ImGui::TextUnformatted(path.c_str());
            ImGui::SameLine();

            if (ImGui::Button("Instantiate##ModelInstantiate")) {
                std::string name = MakeNameFromPath(path);
                Entity e = scene.CreateEntity(name.c_str());
                e.AddComponent<MeshRendererComponent>(h);
                sceneMgr.MarkDirty();

                selectedUUID = e.GetComponent<IDComponent>().ID;
                SyncSelection();
            }

            // Drag source for viewport drop
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::Text("Model: %s", path.c_str());
                AssetHandle payload = h;
                ImGui::SetDragDropPayload("ASSET_MODEL_HANDLE", &payload, sizeof(AssetHandle));
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::Text("Shaders: %d", (int)s_ShaderAssets.size());

        ImGui::End();

        // --- Global hotkeys ---
        if (!io.WantCaptureKeyboard) {
            // Gizmo hotkeys should work even when Hierarchy/Inspector focused
            if (selectedEntity) {
                if (ImGui::IsKeyPressed(ImGuiKey_G)) { gizmo = GizmoMode::Translate; dragging = false; }
                if (ImGui::IsKeyPressed(ImGuiKey_R)) { gizmo = GizmoMode::Rotate;    dragging = false; }
                if (ImGui::IsKeyPressed(ImGuiKey_F)) { gizmo = GizmoMode::Scale;     dragging = false; }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    gizmo = GizmoMode::None;
                    axis = AxisConstraint::None;
                    dragging = false;
                }

                // axis constraint toggles (optional but matches viewport behavior)
                if (ImGui::IsKeyPressed(ImGuiKey_X)) axis = (axis == AxisConstraint::X) ? AxisConstraint::None : AxisConstraint::X;
                if (ImGui::IsKeyPressed(ImGuiKey_Y)) axis = (axis == AxisConstraint::Y) ? AxisConstraint::None : AxisConstraint::Y;
                if (ImGui::IsKeyPressed(ImGuiKey_Z)) axis = (axis == AxisConstraint::Z) ? AxisConstraint::None : AxisConstraint::Z;
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                if (!sceneMgr.Save()) {
                    std::string guess = sceneMgr.GetCurrentPath().empty()
                        ? "Assets/Scenes/NewScene.scene"
                        : sceneMgr.GetCurrentPath();
                    std::snprintf(saveAsBuf, sizeof(saveAsBuf), "%s", guess.c_str());
                    ImGui::OpenPopup("Save As");
                }
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                cmdStack.Undo(scene);
                sceneMgr.MarkDirty();
                SyncSelection();
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                cmdStack.Redo(scene);
                sceneMgr.MarkDirty();
                SyncSelection();
            }
        }

        // --- Scene Browser panel (optional but helpful) ---
        {
            ImGui::Begin("Scene Browser");
            ImGui::Text("Current: %s%s",
                sceneMgr.GetDisplayName().c_str(),
                sceneMgr.IsDirty() ? " *" : "");

            if (ImGui::Button("Refresh")) sceneMgr.RefreshSceneList();
            ImGui::Separator();

            for (const auto& s : sceneMgr.GetSceneList()) {
                bool isCurrent = (s == sceneMgr.GetCurrentPath());
                if (ImGui::Selectable(s.c_str(), isCurrent)) {
                    if (sceneMgr.IsDirty()) {
                        pending = PendingAction::OpenScene;
                        pendingOpenPath = s;
                        ImGui::OpenPopup("Unsaved Changes");
                    }
                    else {
                        sceneMgr.OpenScene(s);
                    }
                }
            }
            if (statusTimer > 0.0f) {
                statusTimer -= dt;
                ImGui::Separator();
                ImGui::TextWrapped("%s", statusText.c_str());
            }
            ImGui::End();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Directional Light")) {
                Entity e = scene.CreateEntity("DirectionalLight");
                auto& dl = e.AddComponent<DirectionalLightComponent>();

                auto& tr = e.GetComponent<TransformComponent>();
                tr.Translation = editorCam.GetPosition() + editorCam.GetForward() * 3.0f;

                // Make the model arrow (-Z) point in the light direction.
                // If your lighting looks reversed, swap dl.Direction -> -dl.Direction.
                tr.Rotation = EulerFromForward(dl.Direction);

                sceneMgr.MarkDirty();
            }

            if (ImGui::MenuItem("Spawn Point")) {
                // Enforce only one SpawnPoint (delete existing)
                {
                    auto view = scene.Registry().view<IDComponent, SpawnPointComponent>();
                    if (view.begin() != view.end()) {
                        // delete first existing spawn point (or all)
                        auto enttHandle = *view.begin();
                        UUID id = view.get<IDComponent>(enttHandle).ID;

                        Entity old = scene.FindEntityByUUID(id);
                        if (old) {
                            auto snap = EditorUndo::CaptureEntity(old);
                            cmdStack.Execute(scene, std::make_unique<EditorUndo::DeleteEntityCommand>(snap));
                            if (selectedUUID == id) ClearSelection();
                        }
                    }
                }
                Entity e = scene.CreateEntity("SpawnPoint");
                e.AddComponent<SpawnPointComponent>();

                auto& tr = e.GetComponent<TransformComponent>();
                tr.Translation = editorCam.GetPosition() + editorCam.GetForward() * 2.0f;
                tr.Rotation = EulerFromForward(editorCam.GetForward()); // arrow (-Z) points camera-forward

                sceneMgr.MarkDirty();
            }

            if (ImGui::MenuItem("Scene Warp")) {
                {
                    auto view = scene.Registry().view<SceneWarpComponent>();
                    int count = 0;
                    for (auto e : view) (void)e, ++count;

                    if (count >= 2) {
                        statusText = "Only 2 SceneWarps allowed for now.";
                        statusTimer = 2.5f;
                    }
                    else {
                        Entity e = scene.CreateEntity("SceneWarp");
                        e.AddComponent<SceneWarpComponent>();
                        e.GetComponent<TransformComponent>().Translation =
                            editorCam.GetPosition() + editorCam.GetForward() * 2.0f;
                        sceneMgr.MarkDirty();
                    }
                }
                Entity e = scene.CreateEntity("SceneWarp");
                e.AddComponent<SceneWarpComponent>(); // default target set in component
                e.GetComponent<TransformComponent>().Translation =
                    editorCam.GetPosition() + editorCam.GetForward() * 2.0f;
                sceneMgr.MarkDirty();
            }

            ImGui::EndMenu();
        }

        // --- Hierarchy ---
        {
            ImGui::Begin("Hierarchy");
            UUID requestDeleteUUID = 0;

            //if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && selectedUUID != 0) {
            //    requestDeleteUUID = selectedUUID;
            ////}

            //UUID requestDeleteUUID = 0; // <-- ADD: defer deletion until after iteration

            auto view = scene.Registry().view<IDComponent, TagComponent>();
            view.each([&](auto ent, IDComponent& idc, TagComponent& tc)
                {
                    ImGui::PushID((void*)(uintptr_t)idc.ID);

                    bool selected = (selectedPickID == FoldUUIDToPickID(idc.ID));
                    if (ImGui::Selectable(tc.Tag.c_str(), selected)) {
                        SelectByUUID(idc.ID);
                    }

                    // --- ADD: Right-click context menu on this item ---
                    if (ImGui::BeginPopupContextItem("EntityContext")) {
                        if (ImGui::MenuItem("Delete")) {
                            requestDeleteUUID = idc.ID; // queue delete
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                });

            // --- ADD: perform delete AFTER iteration ---
            if (requestDeleteUUID != 0) {
                Entity e = scene.FindEntityByUUID(requestDeleteUUID);
                if (e) {
                    auto snap = EditorUndo::CaptureEntity(e);
                    cmdStack.Execute(scene, std::make_unique<EditorUndo::DeleteEntityCommand>(snap));
                    sceneMgr.MarkDirty();

                    if (selectedUUID == requestDeleteUUID)
                        ClearSelection();
                    else
                        SyncSelection(); // selection might still reference something else
                }
            }

            ImGui::End();
        }

        // --- Inspector ---
        {
            ImGui::Begin("Inspector");
            if (selectedEntity) {
                auto& tag = selectedEntity.GetComponent<TagComponent>().Tag;
                ImGui::Text("Entity: %s", tag.c_str());
                ImGui::Text("Scene: %s", sceneMgr.GetDisplayName().c_str());
                ImGui::Text("Dirty: %s", sceneMgr.IsDirty() ? "Yes" : "No");
                ImGui::Text("Axis: %s", AxisName(axis));

                auto& tr = selectedEntity.GetComponent<TransformComponent>();
                /*ImGui::Separator();
                ImGui::Text("MeshRenderer");

                auto& assets = AssetManager::Get();

                static std::vector<std::string> diskModels;*/
                /*if (ImGui::Button("Scan Assets/Models")) {
                    ScanModelsOnDisk("Assets/Models", diskModels);
                }*/

                //for (auto& p : diskModels) {
                //    if (ImGui::Selectable(p.c_str())) {
                //        std::snprintf(importModelPathBuf, sizeof(importModelPathBuf), "%s", p.c_str());
                //    }
                //}

                //// Build model list (reuse the same cached list used by Assets panel)
                //if (ImGui::Button("Refresh Model List")) {
                //    BuildAssetLists(s_ModelAssets, s_ShaderAssets);
                //}

                //if (selectedEntity.HasComponent<MeshRendererComponent>()) {
                //    auto& mrc = selectedEntity.GetComponent<MeshRendererComponent>();

                //    const AssetMetadata* meta = assets.Registry().Get(mrc.Model);
                //    const char* currentLabel = meta ? meta->Path.c_str() : "<None>";

                //    if (ImGui::BeginCombo("Model", currentLabel)) {
                //        // None option
                //        {
                //            bool sel = (mrc.Model == InvalidAssetHandle);
                //            if (ImGui::Selectable("<None>", sel)) {
                //                mrc.Model = InvalidAssetHandle;
                //                sceneMgr.MarkDirty();
                //            }
                //        }

                //        for (auto& [h, path] : s_ModelAssets) {
                //            bool sel = (h == mrc.Model);
                //            if (ImGui::Selectable(path.c_str(), sel)) {
                //                mrc.Model = h;
                //                sceneMgr.MarkDirty();
                //            }
                //            if (sel) ImGui::SetItemDefaultFocus();
                //        }
                //        ImGui::EndCombo();
                //    }

                //    if (ImGui::Button("Remove MeshRenderer")) {
                //        selectedEntity.RemoveComponent<MeshRendererComponent>();
                //        sceneMgr.MarkDirty();
                //    }
                //}
                //else {
                //    if (ImGui::Button("Add MeshRenderer")) {
                //        if (!s_ModelAssets.empty()) {
                //            selectedEntity.AddComponent<MeshRendererComponent>(s_ModelAssets[0].first);
                //            sceneMgr.MarkDirty();
                //        }
                //    }
                //}
                ImGui::Separator();
                // UI for warp and spawn
                if (selectedEntity.HasComponent<SpawnPointComponent>()) {
                    ImGui::Separator();
                    ImGui::Text("Spawn Point");
                    ImGui::TextDisabled("Used by runtime to place player later.");
                }

                if (selectedEntity.HasComponent<SceneWarpComponent>()) {
                    ImGui::Separator();
                    ImGui::Text("Scene Warp");
                    auto& sw = selectedEntity.GetComponent<SceneWarpComponent>();

                    static char targetBuf[260];
                    std::snprintf(targetBuf, sizeof(targetBuf), "%s", sw.TargetScene.c_str());

                    if (ImGui::InputText("Target Scene", targetBuf, sizeof(targetBuf))) {
                        sw.TargetScene = targetBuf;
                        sceneMgr.MarkDirty();
                    }
                }


                ImGui::Separator();
                ImGui::Text("Transform");

                // Translation
                ImGui::DragFloat3("Translation", &tr.Translation.x, 0.05f);
                if (ImGui::IsItemActivated()) inspectorBefore = EditorUndo::CaptureTransform(selectedEntity);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto after = EditorUndo::CaptureTransform(selectedEntity);
                    if (!EditorUndo::TransformEqual(inspectorBefore, after)) {
                        cmdStack.Commit(std::make_unique<EditorUndo::TransformCommand>(selectedUUID, inspectorBefore, after));
                        sceneMgr.MarkDirty();
                    }
                }

                // Rotation (degrees UI)
                float rotDeg[3] = {
                    tr.Rotation.x * 57.29578f,
                    tr.Rotation.y * 57.29578f,
                    tr.Rotation.z * 57.29578f
                };
                bool rotChanged = ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f);
                if (rotChanged) {
                    tr.Rotation.x = rotDeg[0] * 0.01745329f;
                    tr.Rotation.y = rotDeg[1] * 0.01745329f;
                    tr.Rotation.z = rotDeg[2] * 0.01745329f;
                }
                if (ImGui::IsItemActivated()) inspectorBefore = EditorUndo::CaptureTransform(selectedEntity);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto after = EditorUndo::CaptureTransform(selectedEntity);
                    if (!EditorUndo::TransformEqual(inspectorBefore, after)) {
                        cmdStack.Commit(std::make_unique<EditorUndo::TransformCommand>(selectedUUID, inspectorBefore, after));
                        sceneMgr.MarkDirty();
                    }
                }

                // Scale
                ImGui::DragFloat3("Scale", &tr.Scale.x, 0.02f);
                if (ImGui::IsItemActivated()) inspectorBefore = EditorUndo::CaptureTransform(selectedEntity);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto after = EditorUndo::CaptureTransform(selectedEntity);
                    if (!EditorUndo::TransformEqual(inspectorBefore, after)) {
                        cmdStack.Commit(std::make_unique<EditorUndo::TransformCommand>(selectedUUID, inspectorBefore, after));
                        sceneMgr.MarkDirty();
                    }
                }

                ImGui::Separator();
                ImGui::Text("Gizmo");
                if (ImGui::Button("Translate (G)")) gizmo = GizmoMode::Translate;
                ImGui::SameLine();
                if (ImGui::Button("Rotate (R)")) gizmo = GizmoMode::Rotate;
                ImGui::SameLine();
                if (ImGui::Button("Scale (F)")) gizmo = GizmoMode::Scale;
                if (ImGui::Button("None (Esc)")) { gizmo = GizmoMode::None; dragging = false; }
                ImGui::Text("Ctrl = snap | X/Y/Z = axis constraint");
            }
            else {
                ImGui::Text("No selection.");
            }
            ImGui::End();
        }

        // --- Viewport ---
        ImGui::Begin("Viewport");

        ImVec2 vpMin = ImGui::GetCursorScreenPos();
        ImVec2 vpSize = ImGui::GetContentRegionAvail();
        if (vpSize.x < 1) vpSize.x = 1;
        if (vpSize.y < 1) vpSize.y = 1;
        ImVec2 vpMax = ImVec2(vpMin.x + vpSize.x, vpMin.y + vpSize.y);

        uint32_t vw = (uint32_t)vpSize.x;
        uint32_t vh = (uint32_t)vpSize.y;

        bool viewportFocused = ImGui::IsWindowFocused();

        // Viewport hotkeys
        if (viewportFocused && !io.WantCaptureKeyboard) {
            if (ImGui::IsKeyPressed(ImGuiKey_G) && selectedEntity) gizmo = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_R) && selectedEntity) gizmo = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_F) && selectedEntity) gizmo = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { gizmo = GizmoMode::None; dragging = false; }

            if (ImGui::IsKeyPressed(ImGuiKey_X)) axis = (axis == AxisConstraint::X) ? AxisConstraint::None : AxisConstraint::X;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) axis = (axis == AxisConstraint::Y) ? AxisConstraint::None : AxisConstraint::Y;
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) axis = (axis == AxisConstraint::Z) ? AxisConstraint::None : AxisConstraint::Z;

            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && selectedEntity) {
                auto snap = EditorUndo::CaptureEntity(selectedEntity);
                cmdStack.Execute(scene, std::make_unique<EditorUndo::DeleteEntityCommand>(snap));
                sceneMgr.MarkDirty();
                ClearSelection();
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && selectedEntity) {
                UUID newID = GenerateUUID();
                auto snap = EditorUndo::MakeDuplicateSnapshot(scene, selectedEntity, newID);
                cmdStack.Execute(scene, std::make_unique<EditorUndo::CreateEntityCommand>(snap));
                sceneMgr.MarkDirty();
                SelectByUUID(newID);
            }
        }

        // Camera control
        bool viewportRectHovered = ImGui::IsMouseHoveringRect(vpMin, vpMax, false);
        bool cameraControl = viewportRectHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right);

        editorCam.SetActive(cameraControl);
        editorCam.OnUpdate(dt);
        glfwSetInputMode(native, GLFW_CURSOR, cameraControl ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        // Render passes
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
        // ---- Editor Markers (3D models, editor-only) ----
        if (markerShader && (markerLight || markerSpawn || markerWarp)) {
            pipeline.BeginOverlayPass();

            glEnable(GL_DEPTH_TEST);     // 3D markers should depth-test
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            Renderer::BeginScene(editorCam.GetCamera());

            auto SubmitModel = [&](const std::shared_ptr<Model>& m, const glm::mat4& xform) {
                if (!m) return;
                for (const auto& sm : m->GetSubMeshes()) {
                    if (!sm.MeshPtr || !sm.MaterialPtr) continue;
                    Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), xform, 0);
                }
                };

            auto& reg = scene.Registry();

            // Light marker
            {
                auto view = reg.view<TransformComponent, DirectionalLightComponent>();
                view.each([&](auto, TransformComponent& tc, DirectionalLightComponent&) {
                    glm::mat4 xform =
                        tc.GetTransform()
                        * markerFix
                        * glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
                    SubmitModel(markerLight, xform);
                    });
            }

            // Spawn marker
            {
                auto view = reg.view<TransformComponent, SpawnPointComponent>();
                view.each([&](auto, TransformComponent& tc, SpawnPointComponent&) {
                    glm::mat4 xform =
                        tc.GetTransform()
                        * markerFix
                        * glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
                    SubmitModel(markerSpawn, xform);
                    });
            }

            // Warp marker
            {
                auto view = reg.view<TransformComponent, SceneWarpComponent>();
                view.each([&](auto, TransformComponent& tc, SceneWarpComponent&) {
                    glm::mat4 xform =
                        tc.GetTransform()
                        * markerFix
                        * glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));
                    SubmitModel(markerWarp, xform);
                    });
            }

            Renderer::EndScene();

            glDisable(GL_BLEND);
            pipeline.EndOverlayPass();
        }

        // Gizmo visuals
        if (selectedEntity && gizmo != GizmoMode::None) {
            std::vector<GizmoVertex> verts;
            verts.reserve(2048);

            glm::vec3 p = selectedEntity.GetComponent<TransformComponent>().Translation;
            float dist = glm::length(p - editorCam.GetPosition());
            if (dist < 1.0f) dist = 1.0f;
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

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            gizmoRenderer.Draw(editorCam.GetCamera(), verts, 1.0f);

            glEnable(GL_DEPTH_TEST);
        }

        pipeline.Compose();

        ImTextureID tex = (ImTextureID)(intptr_t)pipeline.GetCompositeTexture();
        ImGui::Image(tex, vpSize, ImVec2(0, 1), ImVec2(1, 0));
        // Accept model drop onto viewport -> instantiate entity
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_MODEL_HANDLE")) {
                if (p->DataSize == sizeof(AssetHandle)) {
                    AssetHandle modelH = *(const AssetHandle*)p->Data;
                    if (modelH != InvalidAssetHandle) {
                        auto& assets = AssetManager::Get();
                        const AssetMetadata* meta = assets.Registry().Get(modelH);

                        std::string name = meta ? MakeNameFromPath(meta->Path) : "Model";
                        Entity e = scene.CreateEntity(name.c_str());
                        e.AddComponent<MeshRendererComponent>(modelH);
                        sceneMgr.MarkDirty();

                        selectedUUID = e.GetComponent<IDComponent>().ID;
                        SyncSelection();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        bool imageHovered = ImGui::IsItemHovered();
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 imgMax = ImGui::GetItemRectMax();
        ImVec2 imgSize = ImVec2(imgMax.x - imgMin.x, imgMax.y - imgMin.y);

        ImVec2 mp = ImGui::GetMousePos();
        float mx = mp.x - imgMin.x;
        float my = mp.y - imgMin.y;

        if (mx < 0) mx = 0; if (my < 0) my = 0;
        if (mx > imgSize.x - 1) mx = imgSize.x - 1;
        if (my > imgSize.y - 1) my = imgSize.y - 1;

        bool ctrlDown = io.KeyCtrl;

        // Start drag / pick
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
                uint32_t pid = pipeline.ReadPickingID((uint32_t)mx, (uint32_t)my);
                SelectByPickID(pid);
            }
        }

        // Apply drag
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
                float scale = 0.0020f * dist;
                glm::vec3 out = dragStartTranslation;

                if (axis == AxisConstraint::None) {
                    glm::vec3 right = camRight; right.y = 0.f;
                    glm::vec3 fwd = camFwd;     fwd.y = 0.f;
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
                float delta = dx * 0.01f;
                auto out = dragStartRotation;

                AxisConstraint ax = (axis == AxisConstraint::None) ? AxisConstraint::Y : axis;
                if (ax == AxisConstraint::X) out.x = dragStartRotation.x + delta;
                if (ax == AxisConstraint::Y) out.y = dragStartRotation.y + delta;
                if (ax == AxisConstraint::Z) out.z = dragStartRotation.z + delta;

                if (ctrlDown) {
                    float step = 3.1415926f / 12.0f;
                    out.x = SnapFloat(out.x, step);
                    out.y = SnapFloat(out.y, step);
                    out.z = SnapFloat(out.z, step);
                }

                tc.Rotation = out;
            }
            else if (gizmo == GizmoMode::Scale) {
                float s = 1.0f + (-dy) * 0.01f;
                if (s < 0.05f) s = 0.05f;

                glm::vec3 out = dragStartScale;

                if (axis == AxisConstraint::None) out = dragStartScale * s;
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

        // Commit transform command on release
        bool releasedLMB = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        if (releasedLMB && dragging && selectedEntity) {
            EditorUndo::TransformSnapshot before{ dragStartTranslation, dragStartRotation, dragStartScale };
            EditorUndo::TransformSnapshot after = EditorUndo::CaptureTransform(selectedEntity);

            if (!EditorUndo::TransformEqual(before, after)) {
                cmdStack.Commit(std::make_unique<EditorUndo::TransformCommand>(selectedUUID, before, after));
                sceneMgr.MarkDirty();
            }

            dragging = false;
        }
        else if (releasedLMB) {
            dragging = false;
        }

        ImGui::End(); // Viewport

        // Render ImGui to screen
        ImGui::Render();

        int displayW, displayH;
        glfwGetFramebufferSize(native, &displayW, &displayH);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.07f, 0.07f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(native);
    }

    // Shutdown
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return 0;
}