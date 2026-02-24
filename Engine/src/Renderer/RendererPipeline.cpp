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
        m_ScreenShader->SetFloat("u_Exposure", m_Exposure);
        m_ScreenShader->SetInt("u_Tonemap", m_Tonemap);
        m_ScreenShader->SetFloat("u_Vignette", m_Vignette);

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

    void RendererPipeline::EnsureShadowResources(uint32_t shadowSize) {
        m_ShadowSize = shadowSize;

        if (m_ShadowFBO == 0) glGenFramebuffers(1, &m_ShadowFBO);
        if (m_ShadowDepthTexArray == 0) glGenTextures(1, &m_ShadowDepthTexArray);

        glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowDepthTexArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowDepthTexArray);

        const bool needAlloc =
            (m_ShadowAllocSize != m_ShadowSize) ||
            (m_ShadowAllocCascades != m_ShadowCascadeCount);

        if (needAlloc) {
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
                m_ShadowSize, m_ShadowSize, m_ShadowCascadeCount,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

            m_ShadowAllocSize = m_ShadowSize;
            m_ShadowAllocCascades = m_ShadowCascadeCount;
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[4] = { 1,1,1,1 };
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);

        glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);
        // NOTE: we attach the *layer* later using glFramebufferTextureLayer(...)
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (!m_ShadowDepthShader) {
            m_ShadowDepthShader = std::make_shared<Shader>("Assets/Shaders/ShadowDepth.shader");
            m_ShadowDepthMaterial = std::make_shared<Material>(m_ShadowDepthShader);
        }
    }

    void RendererPipeline::BeginShadowPass(uint32_t shadowSize,
        const glm::mat4& lightViewProj,
        uint32_t cascadeIndex,
        uint32_t cascadeCount)
    {
        m_ShadowCascadeCount = std::min<uint32_t>(cascadeCount, MaxCascades);
        EnsureShadowResources(shadowSize);

        if (m_ShadowCascadeCount == 0)
            return;

        // IMPORTANT: clamp layer index
        cascadeIndex = std::min(cascadeIndex, m_ShadowCascadeCount - 1);

        glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowFBO);

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            m_ShadowDepthTexArray, 0, (GLint)cascadeIndex);

        // IMPORTANT: verify the FBO is actually valid
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            // put your logging/assert here
            // e.g. std::cout << "Shadow FBO incomplete: " << status << "\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        glViewport(0, 0, m_ShadowSize, m_ShadowSize);

        glDisable(GL_BLEND);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // IMPORTANT: force clear depth to 1 (so a cleared map samples as white)
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);

        // DEBUG: disable culling until you see non-empty depth
        //glDisable(GL_CULL_FACE);
        // later, re-enable and choose one:
        // glEnable(GL_CULL_FACE);
        // glCullFace(GL_BACK);  // safer default
        // (then maybe GL_FRONT once it's working)
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // common for shadow maps to reduce self-shadowing (FRONT OR BACK)

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 2.0f); // tweak if needed

        Renderer::BeginScene(lightViewProj);
        m_ShadowPassActive = true;
    }

    void RendererPipeline::EndShadowPass() {
        if (!m_ShadowPassActive) return;

        Renderer::EndScene();
        m_ShadowPassActive = false;

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

} // namespace Engine