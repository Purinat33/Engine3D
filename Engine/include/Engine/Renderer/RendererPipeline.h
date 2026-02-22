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

        // --- Scene pass (renders into offscreen scene framebuffer) ---
        void BeginScenePass(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndScenePass();

        // --- Present (optional; draws fullscreen to default framebuffer) ---
        void PresentToScreen();

        // Convenience if you still want the old behavior in Sandbox:
        // BeginScenePass + EndScenePass + PresentToScreen
        void RenderToScreen(uint32_t width, uint32_t height, const PerspectiveCamera& camera);

        // --- Picking pass ---
        void BeginPickingPass(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndPickingPass();
        uint32_t ReadPickingID(uint32_t mouseX, uint32_t mouseY) const; // mouse coords top-left origin

        std::shared_ptr<Material> GetIDMaterial() const { return m_IDMaterial; }

        // --- Textures for editor UI ---
        uint32_t GetSceneColorTexture() const;
        uint32_t GetIDTexture() const;

        // --- Selection outline input (Screen shader uses this) ---
        void SetSelectedID(uint32_t id) { m_SelectedID = id; }
        uint32_t GetSelectedID() const { return m_SelectedID; }

    private:
        void EnsureSceneResources(uint32_t width, uint32_t height);
        void EnsurePickingResources(uint32_t width, uint32_t height);
        void DrawFullscreen();

    private:
        // Scene
        std::unique_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<Shader> m_ScreenShader;
        std::shared_ptr<VertexArray> m_ScreenQuadVAO;

        // Picking
        std::unique_ptr<Framebuffer> m_IDFB;
        std::shared_ptr<Shader> m_IDShader;
        std::shared_ptr<Material> m_IDMaterial;

        uint32_t m_Width = 0, m_Height = 0;

        bool m_ScenePassActive = false;
        bool m_PickingPassActive = false;

        uint32_t m_SelectedID = 0;
    };

} // namespace Engine