#pragma once
#include <memory>
#include <cstdint>

namespace Engine {

    class Framebuffer;
    class Shader;
    class VertexArray;
    class PerspectiveCamera;
    class Material;

    class RendererPipeline {
    public:
        RendererPipeline();
        ~RendererPipeline();

        // Scene pass + present (existing flow)
        void BeginFrame(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndFrame(); // Ends scene + presents

        // Picking pass
        void BeginPickingPass(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndPickingPass();
        uint32_t ReadPickingID(uint32_t mouseX, uint32_t mouseY) const; // mouse coords top-left origin

        std::shared_ptr<Material> GetIDMaterial() const { return m_IDMaterial; }
        void SetSelectedID(uint32_t id) { m_SelectedID = id; }
        uint32_t GetSelectedID() const { return m_SelectedID; }
    private:
        void Present();
        void DrawFullscreen();

    private:
        uint32_t m_SelectedID = 0;
        std::unique_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<Shader> m_ScreenShader;
        std::shared_ptr<VertexArray> m_ScreenQuadVAO;

        std::unique_ptr<Framebuffer> m_IDFB;
        std::shared_ptr<Shader> m_IDShader;
        std::shared_ptr<Material> m_IDMaterial;

        uint32_t m_Width = 0, m_Height = 0;
    };

} // namespace Engine