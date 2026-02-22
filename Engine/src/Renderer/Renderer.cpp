#include "pch.h"
#include "Engine/Renderer/Renderer.h"

#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

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

    void Renderer::Submit(const std::shared_ptr<Shader>& shader,
        const std::shared_ptr<VertexArray>& vao,
        const glm::mat4& model,
        const glm::vec4& color) {
        // Sort by ShaderID then VAO ID to reduce binds
        uint64_t key = (uint64_t(shader->GetRendererID()) << 32) | uint64_t(vao->GetRendererID());

        DrawCommand cmd;
        cmd.SortKey = key;
        cmd.ShaderPtr = shader;
        cmd.VaoPtr = vao;
        cmd.Model = model;
        cmd.Color = color;
        s_DrawList.push_back(std::move(cmd));
    }

    void Renderer::EndScene() {
        std::sort(s_DrawList.begin(), s_DrawList.end(),
            [](const DrawCommand& a, const DrawCommand& b) { return a.SortKey < b.SortKey; });
        Flush();
    }

    void Renderer::Flush() {
        for (const auto& cmd : s_DrawList) {
            cmd.ShaderPtr->Bind();
            cmd.ShaderPtr->SetMat4("u_ViewProjection", glm::value_ptr(s_ViewProjection));
            cmd.ShaderPtr->SetMat4("u_Model", glm::value_ptr(cmd.Model));

            cmd.ShaderPtr->SetFloat4("u_Color", cmd.Color.r, cmd.Color.g, cmd.Color.b, cmd.Color.a);

            cmd.VaoPtr->Bind();
            RenderCommand::DrawIndexed(cmd.VaoPtr->GetIndexBuffer()->GetCount());
        }
    }

} // namespace Engine