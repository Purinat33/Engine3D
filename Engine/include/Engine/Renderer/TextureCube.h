#pragma once
#include <string>
#include <array>
#include <cstdint>

namespace Engine {

    class TextureCube {
    public:
        // faces order: +X, -X, +Y, -Y, +Z, -Z
        TextureCube(const std::array<std::string, 6>& faces);
        ~TextureCube();

        void Bind(uint32_t slot = 0) const;
        uint32_t GetRendererID() const { return m_RendererID; }

    private:
        uint32_t m_RendererID = 0;
    };

} // namespace Engine