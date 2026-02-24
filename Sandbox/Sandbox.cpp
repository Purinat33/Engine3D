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
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine;

static constexpr int CSM_CASCADES = 4;
static constexpr uint32_t SHADOW_SIZE = 2048;

static glm::mat4 ComputeDirLightMatrix(const glm::vec3& lightDir, const glm::vec3& focusPoint) {
    glm::vec3 dir = glm::normalize(lightDir);
    glm::vec3 up = (std::abs(dir.y) > 0.99f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);

    float dist = 40.0f;
    glm::vec3 lightPos = focusPoint - dir * dist;

    glm::mat4 lightView = glm::lookAt(lightPos, focusPoint, up);

    float orthoSize = 30.0f;     // tweak
    float nearPlane = 1.0f;
    float farPlane = 120.0f;

    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
    return lightProj * lightView;
}

static std::array<glm::vec3, 8> GetFrustumCornersWS(
    const Engine::PerspectiveCamera& cam,
    float nearPlane, float farPlane)
{
    // Rebuild a projection for this slice
    glm::mat4 proj = glm::perspective(cam.GetFov(), cam.GetAspect(), nearPlane, farPlane);
    glm::mat4 inv = glm::inverse(proj * cam.GetView());

    std::array<glm::vec3, 8> corners{};
    int idx = 0;

    // OpenGL NDC z: -1 (near) to +1 (far)
    for (int z = 0; z < 2; z++) {
        float ndcZ = (z == 0) ? -1.0f : 1.0f;
        for (int y = 0; y < 2; y++) {
            float ndcY = (y == 0) ? -1.0f : 1.0f;
            for (int x = 0; x < 2; x++) {
                float ndcX = (x == 0) ? -1.0f : 1.0f;

                glm::vec4 p = inv * glm::vec4(ndcX, ndcY, ndcZ, 1.0f);
                corners[idx++] = glm::vec3(p) / p.w;
            }
        }
    }
    return corners;
}

static glm::mat4 BuildCascadeLightMatrix(
    const Engine::PerspectiveCamera& cam,
    const glm::vec3& lightDir,
    float sliceNear, float sliceFar,
    uint32_t shadowSize)
{
    glm::vec3 dir = glm::normalize(lightDir);
    glm::vec3 up = (std::abs(dir.y) > 0.99f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);

    auto corners = GetFrustumCornersWS(cam, sliceNear, sliceFar);

    // Center of slice
    glm::vec3 center(0.0f);
    for (auto& c : corners) center += c;
    center /= 8.0f;

    // Choose a distance based on slice radius
    float radius = 0.0f;
    for (auto& c : corners) radius = std::max(radius, glm::length(c - center));

    float lightDist = radius + 50.0f;
    glm::vec3 lightPos = center - dir * lightDist;

    glm::mat4 lightView = glm::lookAt(lightPos, center, up);

    // Fit ortho bounds in light space
    glm::vec3 minLS(FLT_MAX), maxLS(-FLT_MAX);
    for (auto& c : corners) {
        glm::vec4 ls = lightView * glm::vec4(c, 1.0f);
        minLS = glm::min(minLS, glm::vec3(ls));
        maxLS = glm::max(maxLS, glm::vec3(ls));
    }

    // Make it square to stabilize
    float extentX = maxLS.x - minLS.x;
    float extentY = maxLS.y - minLS.y;
    float maxExtent = std::max(extentX, extentY);

    float cx = 0.5f * (minLS.x + maxLS.x);
    float cy = 0.5f * (minLS.y + maxLS.y);

    minLS.x = cx - 0.5f * maxExtent;
    maxLS.x = cx + 0.5f * maxExtent;
    minLS.y = cy - 0.5f * maxExtent;
    maxLS.y = cy + 0.5f * maxExtent;

    // Snap to texel grid (reduces shimmering)
    float texel = maxExtent / float(shadowSize);
    minLS.x = std::floor(minLS.x / texel) * texel;
    minLS.y = std::floor(minLS.y / texel) * texel;
    maxLS.x = minLS.x + texel * float(shadowSize);
    maxLS.y = minLS.y + texel * float(shadowSize);

    // Z range (add margin)
    float zMargin = 50.0f;
    float nearLS = minLS.z - zMargin;
    float farLS = maxLS.z + zMargin;

    glm::mat4 lightProj = glm::ortho(minLS.x, maxLS.x, minLS.y, maxLS.y, nearLS, farLS);
    return lightProj * lightView;
}

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

        glm::vec3 lightDir, lightColor;
        bool hasLight = scene.GetMainDirectionalLight(lightDir, lightColor);
        if (!hasLight) { lightDir = { 0.4f,0.8f,-0.3f }; lightColor = { 1,1,1 }; }

        glm::mat4 lightMats[CSM_CASCADES];
        float splits[CSM_CASCADES];

        // quick splits (tweak later)
        splits[0] = 15.0f;
        splits[1] = 40.0f;
        splits[2] = 90.0f;
        splits[3] = 200.0f;

        // build matrices per cascade (still uses your helper for now)
        const PerspectiveCamera& pc = cam.GetCamera();
        float camNear = pc.GetNearClip();
        float camFar = pc.GetFarClip();

        // clamp last split to camera far
        splits[CSM_CASCADES - 1] = std::min(splits[CSM_CASCADES - 1], camFar);

        for (int i = 0; i < CSM_CASCADES; i++) {
            float sliceNear = (i == 0) ? camNear : splits[i - 1];
            float sliceFar = splits[i];

            lightMats[i] = BuildCascadeLightMatrix(pc, lightDir, sliceNear, sliceFar, SHADOW_SIZE);
        }

        // render each cascade into its layer
        for (int i = 0; i < CSM_CASCADES; i++) {
            pipeline.BeginShadowPass(SHADOW_SIZE, lightMats[i], (uint32_t)i, CSM_CASCADES);
            scene.OnRenderShadow(pipeline.GetShadowDepthMaterial());
            pipeline.EndShadowPass();
        }

        pipeline.BeginScenePass(w, h, cam.GetCamera());
        Renderer::SetDirectionalLight(lightDir, lightColor);
        Renderer::SetCSMShadowMap(pipeline.GetShadowDepthTextureArray(), lightMats, splits, CSM_CASCADES);

        scene.OnUpdate(dt);


        scene.OnRender(cam.GetCamera());
        pipeline.EndScenePass();

        pipeline.PresentToScreen();
        window->OnUpdate();
    }

    return 0;
}