#include "pch.h"
#include "Engine/Renderer/TextureCube.h"

#include <glad/glad.h>

// If your Texture2D already uses stb_image, include the same header here.
// Most projects have: #include <stb_image.h>
#include <stb_image.h>

#include <iostream>

namespace Engine {

    TextureCube::TextureCube(const std::array<std::string, 6>& faces) {
        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

        stbi_set_flip_vertically_on_load(false); // cubemap faces should not be flipped

        for (int i = 0; i < 6; i++) {
            int w, h, c;
            unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &c, 0);
            if (!data) {
                std::cout << "[TextureCube] Failed to load: " << faces[i] << "\n";
                continue;
            }

            GLenum fmt = (c == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    TextureCube::~TextureCube() {
        if (m_RendererID) glDeleteTextures(1, &m_RendererID);
    }

    void TextureCube::Bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    }

} // namespace Engine