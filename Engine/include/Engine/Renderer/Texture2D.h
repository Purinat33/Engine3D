#pragma once
#include <string>
#include <cstdint>

namespace Engine {

    class Texture2D {
    public:
        explicit Texture2D(const std::string& path);
        ~Texture2D();

        void Bind(uint32_t slot = 0) const;

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetRendererID() const { return m_RendererID; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Width = 0, m_Height = 0;
        std::string m_Path;
    };

} // namespace Engine