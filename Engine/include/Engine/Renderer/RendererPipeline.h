#pragma once
#include <memory>

namespace Engine {

    class Framebuffer;
    class Shader;
    class VertexArray;
    class PerspectiveCamera;

    class RendererPipeline {
    public:
        RendererPipeline();

        // Called every frame
        void BeginFrame(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndFrame();

        // Offscreen output for debugging/post (optional)
        uint32_t GetSceneColorTexture() const;

    private:
        void DrawFullscreen();

    private:
        std::unique_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<Shader> m_ScreenShader;
        std::shared_ptr<VertexArray> m_ScreenQuadVAO;

        uint32_t m_Width = 0, m_Height = 0;
    };

} // namespace Engine