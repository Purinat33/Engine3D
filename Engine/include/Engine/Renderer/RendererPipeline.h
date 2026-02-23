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

        // --- Picking pass ---
        void BeginPickingPass(uint32_t width, uint32_t height, const PerspectiveCamera& camera);
        void EndPickingPass();
        uint32_t ReadPickingID(uint32_t mouseX, uint32_t mouseY) const; // mouse coords top-left origin
        std::shared_ptr<Material> GetIDMaterial() const { return m_IDMaterial; }

        // --- Composition (Scene + ID outline into a color texture for UI) ---
        void Compose();                          // writes into Composite framebuffer
        uint32_t GetCompositeTexture() const;    // show this in ImGui viewport

        // Optional fullscreen present (Sandbox can still use it)
        void PresentToScreen();

        // --- Selection outline input (used by Screen.shader) ---
        void SetSelectedID(uint32_t id) { m_SelectedID = id; }
        uint32_t GetSelectedID() const { return m_SelectedID; }

        void BeginOverlayPass(); // binds scene framebuffer, no clear
        void EndOverlayPass();   // currently no-op
        float m_Exposure = 1.0f;
        int   m_Tonemap = 2;     // ACES default
        float m_Vignette = 0.0f;
    private:
        void EnsureSceneResources(uint32_t width, uint32_t height);
        void EnsurePickingResources(uint32_t width, uint32_t height);
        void EnsureCompositeResources(uint32_t width, uint32_t height);
        void DrawFullscreen(); // draws Screen.shader using Scene + ID

    private:
        // Scene
        std::unique_ptr<Framebuffer> m_SceneFB;
        std::shared_ptr<Shader> m_ScreenShader;
        std::shared_ptr<VertexArray> m_ScreenQuadVAO;

        // Picking
        std::unique_ptr<Framebuffer> m_IDFB;
        std::shared_ptr<Shader> m_IDShader;
        std::shared_ptr<Material> m_IDMaterial;

        // Composite (for ImGui viewport)
        std::unique_ptr<Framebuffer> m_CompositeFB;

        uint32_t m_Width = 0, m_Height = 0;
        bool m_ScenePassActive = false;
        bool m_PickingPassActive = false;
        uint32_t m_SelectedID = 0;
    };

} // namespace Engine