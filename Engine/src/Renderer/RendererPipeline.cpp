#include "pch.h"
#include "Engine/Renderer/RendererPipeline.h"

#include "Engine/Renderer/Framebuffer.h"
#include "Engine/Renderer/RenderCommand.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/ScreenQuad.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/PerspectiveCamera.h"
#include "Engine/Renderer/Material.h"

#include <glad/glad.h>
#include <algorithm>

namespace Engine {

    RendererPipeline::~RendererPipeline() = default;

    RendererPipeline::RendererPipeline() {
        m_ScreenQuadVAO = ScreenQuad::GetVAO();
    }

    void RendererPipeline::BeginFrame(uint32_t width, uint32_t height, const PerspectiveCamera& camera) {
        m_Width = width;
        m_Height = height;

        if (!m_SceneFB)
            m_SceneFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height, FramebufferColorFormat::RGBA8 });
        else
            m_SceneFB->Resize(width, height);

        if (!m_ScreenShader)
            m_ScreenShader = std::make_shared<Shader>("Assets/Shaders/Screen.shader");

        m_SceneFB->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        RenderCommand::Clear();

        Renderer::BeginScene(camera);
    }

    void RendererPipeline::EndFrame() {
        Renderer::EndScene();
        Present();
    }

    void RendererPipeline::BeginPickingPass(uint32_t width, uint32_t height, const PerspectiveCamera& camera) {
        m_Width = width;
        m_Height = height;

        if (!m_IDFB)
            m_IDFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height, FramebufferColorFormat::R32UI });
        else
            m_IDFB->Resize(width, height);

        if (!m_IDShader) {
            m_IDShader = std::make_shared<Shader>("Assets/Shaders/ID.shader");
            m_IDMaterial = std::make_shared<Material>(m_IDShader);
            m_IDMaterial->SetColor({ 1,1,1,1 }); // harmless
        }

        m_IDFB->Bind();
        RenderCommand::SetViewport(0, 0, width, height);

        // Clear ID buffer to 0 = "no entity"
        m_IDFB->ClearUInt(0);

        Renderer::BeginScene(camera);
    }

    void RendererPipeline::EndPickingPass() {
        Renderer::EndScene();
    }

    uint32_t RendererPipeline::ReadPickingID(uint32_t mouseX, uint32_t mouseY) const {
        if (!m_IDFB) return 0;

        // Convert from top-left origin (mouse) to bottom-left (OpenGL)
        if (mouseX >= m_Width || mouseY >= m_Height) return 0;
        uint32_t x = mouseX;
        uint32_t y = (m_Height - 1) - mouseY;

        return m_IDFB->ReadPixelUInt(x, y);
    }

    void RendererPipeline::Present() {
        Framebuffer::BindDefault();
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
        RenderCommand::SetClearColor(0.f, 0.f, 0.f, 1.f);
        RenderCommand::Clear();

        glDisable(GL_DEPTH_TEST);
        DrawFullscreen();
        glEnable(GL_DEPTH_TEST);
    }

    void RendererPipeline::DrawFullscreen() {
        m_ScreenShader->Bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_SceneFB->GetColorAttachmentRendererID());
        m_ScreenShader->SetInt("u_Scene", 0);

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO->GetIndexBuffer()->GetCount());
    }

} // namespace Engine