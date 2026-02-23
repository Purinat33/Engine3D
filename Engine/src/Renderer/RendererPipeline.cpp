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

namespace Engine {

    RendererPipeline::~RendererPipeline() = default;

    RendererPipeline::RendererPipeline() {
        m_ScreenQuadVAO = ScreenQuad::GetVAO();
    }

    void RendererPipeline::EnsureSceneResources(uint32_t width, uint32_t height) {
        m_Width = width;
        m_Height = height;

        if (!m_SceneFB)
            m_SceneFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height, FramebufferColorFormat::RGBA8 });
        else
            m_SceneFB->Resize(width, height);

        if (!m_ScreenShader)
            m_ScreenShader = std::make_shared<Shader>("Assets/Shaders/Screen.shader");
    }

    void RendererPipeline::EnsurePickingResources(uint32_t width, uint32_t height) {
        m_Width = width;
        m_Height = height;

        if (!m_IDFB)
            m_IDFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height, FramebufferColorFormat::R32UI });
        else
            m_IDFB->Resize(width, height);

        if (!m_IDShader) {
            m_IDShader = std::make_shared<Shader>("Assets/Shaders/ID.shader");
            m_IDMaterial = std::make_shared<Material>(m_IDShader);
            m_IDMaterial->SetColor({ 1,1,1,1 });
        }
    }

    void RendererPipeline::EnsureCompositeResources(uint32_t width, uint32_t height) {
        m_Width = width;
        m_Height = height;

        if (!m_CompositeFB)
            m_CompositeFB = std::make_unique<Framebuffer>(FramebufferSpec{ width, height, FramebufferColorFormat::RGBA8 });
        else
            m_CompositeFB->Resize(width, height);
    }

    // ---------------- Scene pass ----------------

    void RendererPipeline::BeginScenePass(uint32_t width, uint32_t height, const PerspectiveCamera& camera) {
        EnsureSceneResources(width, height);

        m_SceneFB->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::SetClearColor(0.08f, 0.10f, 0.12f, 1.0f);
        RenderCommand::Clear();

        Renderer::BeginScene(camera);
        m_ScenePassActive = true;
    }

    void RendererPipeline::EndScenePass() {
        if (!m_ScenePassActive) return;
        Renderer::EndScene();
        m_ScenePassActive = false;
    }

    // ---------------- Picking pass ----------------

    void RendererPipeline::BeginPickingPass(uint32_t width, uint32_t height, const PerspectiveCamera& camera) {
        EnsurePickingResources(width, height);

        m_IDFB->Bind();
        RenderCommand::SetViewport(0, 0, width, height);

        m_IDFB->ClearUInt(0);
        Renderer::BeginScene(camera);
        m_PickingPassActive = true;
    }

    void RendererPipeline::EndPickingPass() {
        if (!m_PickingPassActive) return;
        Renderer::EndScene();
        m_PickingPassActive = false;
    }

    uint32_t RendererPipeline::ReadPickingID(uint32_t mouseX, uint32_t mouseY) const {
        if (!m_IDFB) return 0;
        if (mouseX >= m_Width || mouseY >= m_Height) return 0;

        uint32_t x = mouseX;
        uint32_t y = (m_Height - 1) - mouseY; // top-left -> bottom-left
        return m_IDFB->ReadPixelUInt(x, y);
    }

    // ---------------- Compose + Present ----------------

    void RendererPipeline::Compose() {
        if (!m_SceneFB || !m_ScreenShader || !m_ScreenQuadVAO) return;
        EnsureCompositeResources(m_Width, m_Height);

        m_CompositeFB->Bind();
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
        RenderCommand::SetClearColor(0.f, 0.f, 0.f, 1.f);
        RenderCommand::Clear();

        glDisable(GL_DEPTH_TEST);
        DrawFullscreen();
        glEnable(GL_DEPTH_TEST);
    }

    uint32_t RendererPipeline::GetCompositeTexture() const {
        return m_CompositeFB ? m_CompositeFB->GetColorAttachmentRendererID() : 0;
    }

    void RendererPipeline::PresentToScreen() {
        if (!m_SceneFB || !m_ScreenShader || !m_ScreenQuadVAO) return;

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

        glActiveTexture(GL_TEXTURE1);
        if (m_IDFB)
            glBindTexture(GL_TEXTURE_2D, m_IDFB->GetColorAttachmentRendererID());
        else
            glBindTexture(GL_TEXTURE_2D, 0);
        m_ScreenShader->SetInt("u_ID", 1);

        m_ScreenShader->SetUInt("u_SelectedID", m_SelectedID);
        m_ScreenShader->SetFloat3("u_OutlineColor", 1.0f, 0.85f, 0.1f);

        m_ScreenQuadVAO->Bind();
        RenderCommand::DrawIndexed(m_ScreenQuadVAO->GetIndexBuffer()->GetCount());
    }

    void RendererPipeline::BeginOverlayPass() {
        if (!m_SceneFB) return;
        m_SceneFB->Bind();
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
    }

    void RendererPipeline::EndOverlayPass() {
        // no-op; scene FB stays bound until Compose() binds its own FB anyway
    }

} // namespace Engine