#include "pch.h"
#include "Engine/Renderer/Renderer.h"

#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/Material.h"

#include "Engine/Renderer/TextureCube.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>

static std::shared_ptr<Engine::Shader> s_SkyboxShader;
static std::shared_ptr<Engine::VertexArray> s_SkyboxVAO;
static std::shared_ptr<Engine::TextureCube> s_SkyboxTex;

static std::shared_ptr<Engine::VertexArray> CreateSkyboxCubeVAO() {
    // 36 verts cube, positions only
    float v[] = {
        // ... (standard cube positions) ...
        -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
        -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
         1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
    };

    auto vao = std::make_shared<Engine::VertexArray>();
    auto vb = std::make_shared<Engine::VertexBuffer>(v, sizeof(v));
    vb->SetLayout({ { Engine::ShaderDataType::Float3 } });
    vao->AddVertexBuffer(vb);
    return vao;
}

namespace Engine {

    glm::mat4 Renderer::s_ViewProjection{ 1.0f };
    std::vector<Renderer::DrawCommand> Renderer::s_DrawList;

    bool Renderer::s_HasDirLight = false;
    glm::vec3 Renderer::s_DirLightDir = glm::vec3(0.4f, 0.8f, -0.3f);
    glm::vec3 Renderer::s_DirLightColor = glm::vec3(1.0f);


    void Renderer::Init() {
        RenderCommand::Init();
    }

    void Renderer::BeginScene(const PerspectiveCamera& camera) {
        s_ViewProjection = camera.GetViewProjection();
        s_DrawList.clear();
        
    }

    void Renderer::EndScene() {
        std::sort(s_DrawList.begin(), s_DrawList.end(),
            [](const DrawCommand& a, const DrawCommand& b) { return a.SortKey < b.SortKey; });
        Flush();
    }

    void Renderer::Flush() {
        for (const auto& cmd : s_DrawList) {
            auto& mat = cmd.MaterialPtr;
            if (!mat) continue;
            auto& shader = mat->GetShader();
            if (!shader) continue;

            shader->Bind();
            shader->SetMat4("u_ViewProjection", glm::value_ptr(s_ViewProjection));
            shader->SetMat4("u_Model", glm::value_ptr(cmd.Model));
            shader->SetUInt("u_EntityID", cmd.EntityID);

            shader->SetInt("u_UseLighting", s_HasDirLight ? 1 : 0);
            shader->SetFloat3("u_LightDir", s_DirLightDir.x, s_DirLightDir.y, s_DirLightDir.z);
            shader->SetFloat3("u_LightColor", s_DirLightColor.x, s_DirLightColor.y, s_DirLightColor.z);
            shader->SetFloat("u_Ambient", 0.07f); // Set Light Level

            // Default: no texture
            int useTexture0 = 0;

            // Bind textures (slot -> u_Texture{slot})
            for (const auto& kv : mat->GetTextures()) {
                uint32_t slot = kv.first;
                const auto& tex = kv.second;
                if (!tex) continue;

                tex->Bind(slot);
                shader->SetInt("u_Texture" + std::to_string(slot), (int)slot);

                if (slot == 0) useTexture0 = 1;
            }

            // Tell shader whether to sample texture0
            shader->SetInt("u_UseTexture", useTexture0);

            if (mat->HasColor()) {
                const auto& c = mat->GetColor();
                shader->SetFloat4("u_Color", c.r, c.g, c.b, c.a);
            }
            else {
                shader->SetFloat4("u_Color", 1.f, 1.f, 1.f, 1.f);
            }

            cmd.VaoPtr->Bind();
            auto count = cmd.VaoPtr->GetIndexBuffer()->GetCount();
            if (count == 0) continue;

            // Two Sided Drawing
            bool restoreCull = false;
            if (mat->IsTwoSided() && glIsEnabled(GL_CULL_FACE)) {
                glDisable(GL_CULL_FACE);
                restoreCull = true;
            }

            RenderCommand::DrawIndexed(count);

            if (restoreCull)
                glEnable(GL_CULL_FACE);
        }


    }

    void Renderer::Submit(const std::shared_ptr<Material>& material,
        const std::shared_ptr<VertexArray>& vao,
        const glm::mat4& model) {
        if (!material || !vao) return;
        if (!material->GetShader()) return;
        Submit(material, vao, model, 0);
    }

    void Renderer::Submit(const std::shared_ptr<Material>& material,
        const std::shared_ptr<VertexArray>& vao,
        const glm::mat4& model,
        uint32_t entityID) {
        uint64_t key = (uint64_t(material->GetShader()->GetRendererID()) << 32) |
            uint64_t(vao->GetRendererID());
        if (!material || !vao) return;
        if (!material->GetShader()) return;

        DrawCommand cmd;
        cmd.SortKey = key;
        cmd.MaterialPtr = material;
        cmd.VaoPtr = vao;
        cmd.Model = model;
        cmd.EntityID = entityID;

        s_DrawList.push_back(std::move(cmd));
    }

    void Renderer::SetDirectionalLight(const glm::vec3& dir, const glm::vec3& color) {
        s_HasDirLight = true;
        s_DirLightDir = glm::normalize(dir);
        s_DirLightColor = color;
    }

    void Renderer::ClearLights() {
        s_HasDirLight = false;
        s_DirLightDir = glm::vec3(0.4f, 0.8f, -0.3f);
        s_DirLightColor = glm::vec3(1.0f);
    }

    void Engine::Renderer::SetSkybox(const std::shared_ptr<TextureCube>& sky) {
        s_SkyboxTex = sky;
        if (!s_SkyboxShader) s_SkyboxShader = std::make_shared<Shader>("Assets/Shaders/Skybox.shader");
        if (!s_SkyboxVAO)    s_SkyboxVAO = CreateSkyboxCubeVAO();
    }

    void Renderer::DrawSkybox(const PerspectiveCamera& camera) {
        if (!s_SkyboxTex || !s_SkyboxShader || !s_SkyboxVAO) return;

        GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullWasEnabled) glDisable(GL_CULL_FACE);

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        s_SkyboxShader->Bind();
        int loc = glGetUniformLocation(s_SkyboxShader->GetRendererID(), "u_ViewProjectionNoTranslate");
        std::cout << "[Skybox] VP loc = " << loc << "\n";
        glm::mat4 view = glm::mat4(glm::mat3(camera.GetView()));
        glm::mat4 vp = camera.GetProjection() * view;
        s_SkyboxShader->SetMat4("u_ViewProjectionNoTranslate", glm::value_ptr(vp));

        s_SkyboxTex->Bind(0);
        s_SkyboxShader->SetInt("u_Skybox", 0);

        s_SkyboxVAO->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        if (cullWasEnabled) glEnable(GL_CULL_FACE);
    }

} // namespace Engine