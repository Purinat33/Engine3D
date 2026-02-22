#include "pch.h"
#include "Engine/Renderer/RendererPipeline.h"

#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/ScreenQuad.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <glad/glad.h>

namespace Engine {

    RendererPipeline::RendererPipeline() {
        m_ScreenQuadVAO = ScreenQuad::GetVAO();
    }

    void RendererPipeline::BeginFrame(uint32_t width, uint32_t height, const PerspectiveCamera& camera) {
        m_Width = width;
        m_Height = height;

        if (!m_SceneFB) {
            m_SceneFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height });
        }
        else {
            m_SceneFB->Resize(width, height);
        }

        if (!m_ScreenShader) {
            // Load from file through your Shader class (uses #type parsing)
            m_ScreenShader = std::make_shared<Shader>("Assets/Shaders/Screen.shader");
        }

        // Scene pass
        m_SceneFB->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        RenderCommand::Clear();

        /*Renderer::BeginScene(camera);*/
    }

    void RendererPipeline::EndFrame() {
        // Finish scene draw list
        Renderer::EndScene();

        // Present pass
        Framebuffer::BindDefault();
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
        RenderCommand::SetClearColor(0.f, 0.f, 0.f, 1.f);
        RenderCommand::Clear();

        DrawFullscreen();
    }

    void RendererPipeline::DrawFullscreen() {
        m_ScreenShader->Bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_SceneFB->GetColorAttachmentRendererID());
        m_ScreenShader->SetInt("u_Scene", 0);

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO->GetIndexBuffer()->GetCount());
    }

    uint32_t RendererPipeline::GetSceneColorTexture() const {
        return m_SceneFB ? m_SceneFB->GetColorAttachmentRendererID() : 0;
    }

} // namespace Engine