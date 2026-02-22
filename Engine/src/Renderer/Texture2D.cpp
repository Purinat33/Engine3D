#include "pch.h"
#include "Engine/Renderer/Texture2D.h"

#include <glad/glad.h>
#include <stb_image.h>
#include <stdexcept>
#include <iostream>

namespace Engine {

    Texture2D::Texture2D(const std::string& path)
        : m_Path(path) {
        stbi_set_flip_vertically_on_load(1);

        int w, h, channelsInFile;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channelsInFile, 4);

        unsigned char fallback[16] = {
            255, 0, 255, 255,   0, 0, 0, 255,
            0, 0, 0, 255,       255, 0, 255, 255
        };

        GLenum internalFormat = GL_RGBA8;
        GLenum dataFormat = GL_RGBA;

        if (data) {
            m_Width = (uint32_t)w;
            m_Height = (uint32_t)h;

            glGenTextures(1, &m_RendererID);
            glBindTexture(GL_TEXTURE_2D, m_RendererID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (int)m_Width, (int)m_Height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(data);
        }
        else {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 2, 2, 0, dataFormat, GL_UNSIGNED_BYTE, fallback);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    Texture2D::~Texture2D() {
        glDeleteTextures(1, &m_RendererID);
    }

    void Texture2D::Bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

} // namespace Engine