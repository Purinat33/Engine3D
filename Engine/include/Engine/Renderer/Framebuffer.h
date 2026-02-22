#pragma once
#include <cstdint>

namespace Engine {

    struct FramebufferSpec {
        uint32_t Width = 1280;
        uint32_t Height = 720;
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

    private:
        void Invalidate();

    private:
        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_DepthAttachment = 0;
        FramebufferSpec m_Spec;
    };

} // namespace Engine