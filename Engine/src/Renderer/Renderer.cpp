#include "pch.h"
#include "Engine/Renderer/Renderer.h"

#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/Material.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include <glad/glad.h>

namespace Engine {

    glm::mat4 Renderer::s_ViewProjection{ 1.0f };
    std::vector<Renderer::DrawCommand> Renderer::s_DrawList;

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
            auto& shader = mat->GetShader();

            shader->Bind();
            shader->SetMat4("u_ViewProjection", glm::value_ptr(s_ViewProjection));
            shader->SetMat4("u_Model", glm::value_ptr(cmd.Model));
            shader->SetUInt("u_EntityID", cmd.EntityID);

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
        Submit(material, vao, model, 0);
    }

    void Renderer::Submit(const std::shared_ptr<Material>& material,
        const std::shared_ptr<VertexArray>& vao,
        const glm::mat4& model,
        uint32_t entityID) {
        uint64_t key = (uint64_t(material->GetShader()->GetRendererID()) << 32) |
            uint64_t(vao->GetRendererID());

        DrawCommand cmd;
        cmd.SortKey = key;
        cmd.MaterialPtr = material;
        cmd.VaoPtr = vao;
        cmd.Model = model;
        cmd.EntityID = entityID;

        s_DrawList.push_back(std::move(cmd));
    }
    
} // namespace Engine