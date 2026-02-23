#include "pch.h"
#include "EditorIconRenderer.h"

#include <Engine/Core/Content.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Components.h>
#include <Engine/Renderer/Shader.h>
#include <Engine/Renderer/Material.h>
#include <Engine/Renderer/Texture2D.h>
#include <Engine/Renderer/VertexArray.h>
#include <Engine/Renderer/Buffer.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/CameraController.h>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine;

struct IconVertex { glm::vec3 Pos; glm::vec3 N; glm::vec2 UV; };

static std::shared_ptr<VertexArray> CreateUnitQuadVAO() {
    // centered quad in XY plane
    IconVertex v[] = {
        { {-0.5f,-0.5f,0.0f},{0,0,1},{0,0} },
        { { 0.5f,-0.5f,0.0f},{0,0,1},{1,0} },
        { { 0.5f, 0.5f,0.0f},{0,0,1},{1,1} },
        { {-0.5f, 0.5f,0.0f},{0,0,1},{0,1} },
    };
    uint32_t idx[] = { 0,1,2, 2,3,0 };

    auto vao = std::make_shared<VertexArray>();
    auto vb = std::make_shared<VertexBuffer>(v, sizeof(v));
    vb->SetLayout(BufferLayout({
        { ShaderDataType::Float3, (uint32_t)offsetof(IconVertex, Pos) },
        { ShaderDataType::Float3, (uint32_t)offsetof(IconVertex, N) },
        { ShaderDataType::Float2, (uint32_t)offsetof(IconVertex, UV) },
        }, (uint32_t)sizeof(IconVertex)));
    vao->AddVertexBuffer(vb);

    auto ib = std::make_shared<IndexBuffer>(idx, 6);
    vao->SetIndexBuffer(ib);
    return vao;
}

void EditorIconRenderer::Init() {
    if (m_Shader) return;

    // Put this shader in EngineContent/Shaders/Icon.shader
    m_Shader = std::make_shared<Shader>(Content::Resolve("Shaders/Icon.shader"));
    m_Quad = CreateUnitQuadVAO();
}

std::shared_ptr<Texture2D> EditorIconRenderer::GetIcon(int type) {
    if (auto it = m_IconTex.find(type); it != m_IconTex.end())
        return it->second;

    std::string rel;
    if (type == (int)EditorIconType::Light)      rel = "Editor/Icons/light.png";
    if (type == (int)EditorIconType::Camera)     rel = "Editor/Icons/camera.png";
    if (type == (int)EditorIconType::SpawnPoint) rel = "Editor/Icons/spawn.png";

    const std::string path = Content::Resolve(rel);
    if (path.empty()) return nullptr;

    try {
        auto tex = std::make_shared<Texture2D>(path);
        m_IconTex[type] = tex;
        return tex;
    }
    catch (...) {
        return nullptr;
    }
}

void EditorIconRenderer::Draw(Scene& scene, CameraController& cam, uint32_t /*selectedPickID*/) {
    if (!m_Shader || !m_Quad) return;

    // billboard basis from camera controller
    glm::vec3 right = cam.GetRight();
    glm::vec3 fwd = cam.GetForward();
    glm::vec3 up = glm::normalize(glm::cross(right, fwd));
    glm::vec3 face = glm::normalize(glm::cross(up, right));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // typical editor icons (always visible)

    Renderer::BeginScene(cam.GetCamera());

    auto view = scene.Registry().view<TransformComponent, EditorIconComponent>();
    view.each([&](auto, TransformComponent& tc, EditorIconComponent& ic) {
        if (ic.Type == EditorIconType::None) return;
        auto tex = GetIcon((int)ic.Type);
        if (!tex) return;

        auto mat = std::make_shared<Material>(m_Shader);
        mat->SetTwoSided(true);
        mat->SetTexture(0, tex);

        float s = ic.Size;
        glm::mat4 M(1.0f);
        M[0] = glm::vec4(right * s, 0.0f);
        M[1] = glm::vec4(up * s, 0.0f);
        M[2] = glm::vec4(face * s, 0.0f);
        M[3] = glm::vec4(tc.Translation, 1.0f);

        Renderer::Submit(mat, m_Quad, M);
        });

    Renderer::EndScene();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}