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

        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);

        if (m_Spec.ColorFormat == FramebufferColorFormat::RGBA8) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)m_Spec.Width, (int)m_Spec.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else {
            // R32UI picking buffer
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, (int)m_Spec.Width, (int)m_Spec.Height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        glGenRenderbuffers(1, &m_DepthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)m_Spec.Width, (int)m_Spec.Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthAttachment);

        GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, drawBuffers);

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

    void Framebuffer::ClearUInt(uint32_t value) const {
        if (!IsIntegerColor()) return;
        Bind();
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glClearBufferuiv(GL_COLOR, 0, &value);
    }

    uint32_t Framebuffer::ReadPixelUInt(uint32_t x, uint32_t y) const {
        if (!IsIntegerColor()) return 0;

        Bind();
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        uint32_t pixel = 0;
        glReadPixels((int)x, (int)y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &pixel);

        return pixel;
    }

} // namespace Engine