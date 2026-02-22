#pragma once
#include <cstdint>

namespace Engine {

    enum class FramebufferColorFormat {
        RGBA8 = 0,
        R32UI
    };

    struct FramebufferSpec {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        FramebufferColorFormat ColorFormat = FramebufferColorFormat::RGBA8;
    };

    class Framebuffer {
    public:
        explicit Framebuffer(const FramebufferSpec& spec);
        ~Framebuffer();

        void Bind() const;
        static void BindDefault();

        void Resize(uint32_t width, uint32_t height);

        uint32_t GetColorAttachmentRendererID() const { return m_ColorAttachment; }
        const FramebufferSpec& GetSpec() const { return m_Spec; }

        // For picking buffers (R32UI)
        void ClearUInt(uint32_t value) const;
        uint32_t ReadPixelUInt(uint32_t x, uint32_t y) const;

    private:
        void Invalidate();
        bool IsIntegerColor() const { return m_Spec.ColorFormat == FramebufferColorFormat::R32UI; }

    private:
        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_DepthAttachment = 0;
        FramebufferSpec m_Spec;
    };

} // namespace Engine