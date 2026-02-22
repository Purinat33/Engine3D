#include "pch.h"
#include "Engine/Renderer/Framebuffer.h"
#include <glad/glad.h>
#include <stdexcept>

namespace Engine {

    Framebuffer::Framebuffer(const FramebufferSpec& spec)
        : m_Spec(spec) {
        Invalidate();
    }

    Framebuffer::~Framebuffer() {
        if (m_DepthAttachment) glDeleteRenderbuffers(1, &m_DepthAttachment);
        if (m_ColorAttachment) glDeleteTextures(1, &m_ColorAttachment);
        if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
    }

    void Framebuffer::Invalidate() {
        if (m_FBO) {
            if (m_DepthAttachment) glDeleteRenderbuffers(1, &m_DepthAttachment);
            if (m_ColorAttachment) glDeleteTextures(1, &m_ColorAttachment);
            glDeleteFramebuffers(1, &m_FBO);
            m_FBO = m_ColorAttachment = m_DepthAttachment = 0;
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // Color texture
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)m_Spec.Width, (int)m_Spec.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        // Depth renderbuffer
        glGenRenderbuffers(1, &m_DepthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)m_Spec.Width, (int)m_Spec.Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthAttachment);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Framebuffer incomplete");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    }

    void Framebuffer::BindDefault() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        if (width == m_Spec.Width && height == m_Spec.Height) return;
        m_Spec.Width = width;
        m_Spec.Height = height;
        Invalidate();
    }

} // namespace Engine