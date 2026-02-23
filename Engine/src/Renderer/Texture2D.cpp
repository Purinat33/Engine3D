#include "pch.h"
#include "Engine/Renderer/Texture2D.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>
#include <vector>

namespace Engine {

    static void SetupSampler2D() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    Texture2D::Texture2D(const std::string& path) {
        m_DebugName = path;

        int w = 0, h = 0, channels = 0;

        // If you already flip textures globally elsewhere, remove this line.
        // Many engines flip for OpenGL.
        stbi_set_flip_vertically_on_load(1);

        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        if (!data)
            throw std::runtime_error("Failed to load texture: " + path);

        // Convert to RGBA8 if needed
        std::vector<uint8_t> rgba;
        const uint8_t* pixelsRGBA = nullptr;

        if (channels == 4) {
            pixelsRGBA = data;
        }
        else if (channels == 3) {
            rgba.resize((size_t)w * (size_t)h * 4);
            for (int i = 0; i < w * h; i++) {
                rgba[i * 4 + 0] = data[i * 3 + 0];
                rgba[i * 4 + 1] = data[i * 3 + 1];
                rgba[i * 4 + 2] = data[i * 3 + 2];
                rgba[i * 4 + 3] = 255;
            }
            pixelsRGBA = rgba.data();
        }
        else {
            stbi_image_free(data);
            throw std::runtime_error("Unsupported texture channels: " + std::to_string(channels) + " in " + path);
        }

        UploadRGBA8(pixelsRGBA, w, h);
        stbi_image_free(data);
    }

    Texture2D::~Texture2D() {
        if (m_RendererID)
            glDeleteTextures(1, &m_RendererID);
    }

    void Texture2D::UploadRGBA8(const uint8_t* rgbaPixels, int width, int height) {
        m_Width = (uint32_t)width;
        m_Height = (uint32_t)height;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        SetupSampler2D();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture2D::Bind(uint32_t slot) const {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    // ---- NEW: Create from compressed bytes (PNG/JPG inside GLB) ----
    Texture2D* Texture2D::CreateFromMemory(const std::string& debugName, const uint8_t* bytes, size_t sizeBytes) {
        int w = 0, h = 0, channels = 0;

        stbi_set_flip_vertically_on_load(1);

        stbi_uc* data = stbi_load_from_memory(bytes, (int)sizeBytes, &w, &h, &channels, 0);
        if (!data)
            throw std::runtime_error("Failed to decode embedded texture: " + debugName);

        std::vector<uint8_t> rgba;
        const uint8_t* pixelsRGBA = nullptr;

        if (channels == 4) {
            pixelsRGBA = data;
        }
        else if (channels == 3) {
            rgba.resize((size_t)w * (size_t)h * 4);
            for (int i = 0; i < w * h; i++) {
                rgba[i * 4 + 0] = data[i * 3 + 0];
                rgba[i * 4 + 1] = data[i * 3 + 1];
                rgba[i * 4 + 2] = data[i * 3 + 2];
                rgba[i * 4 + 3] = 255;
            }
            pixelsRGBA = rgba.data();
        }
        else {
            stbi_image_free(data);
            throw std::runtime_error("Unsupported embedded texture channels: " + std::to_string(channels));
        }

        Texture2D* tex = new Texture2D();
        tex->m_DebugName = debugName;
        tex->UploadRGBA8(pixelsRGBA, w, h);

        stbi_image_free(data);
        return tex;
    }

    // ---- NEW: Create from raw RGBA8 pixels (rare case: uncompressed aiTexture) ----
    Texture2D* Texture2D::CreateFromRGBA8(const std::string& debugName, const uint8_t* rgbaPixels, int width, int height) {
        Texture2D* tex = new Texture2D();
        tex->m_DebugName = debugName;
        tex->UploadRGBA8(rgbaPixels, width, height);
        return tex;
    }

} // namespace Engine