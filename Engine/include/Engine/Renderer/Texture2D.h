#pragma once
#include <string>
#include <cstdint>

namespace Engine {

    class Texture2D {
    public:
        Texture2D(const std::string& path);

        // NEW: for GLB / embedded textures
        static Texture2D* CreateFromMemory(const std::string& debugName, const uint8_t* bytes, size_t sizeBytes);
        static Texture2D* CreateFromRGBA8(const std::string& debugName, const uint8_t* rgbaPixels, int width, int height);

        ~Texture2D();

        void Bind(uint32_t slot = 0) const;
        uint32_t GetRendererID() const { return m_RendererID; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        Texture2D() = default; // used by CreateFromMemory/CreateFromRGBA8
        void UploadRGBA8(const uint8_t* rgbaPixels, int width, int height);

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Width = 0, m_Height = 0;
        std::string m_DebugName;
    };

} // namespace Engine