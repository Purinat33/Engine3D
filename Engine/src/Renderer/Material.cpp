#include "pch.h"
#include "Engine/Renderer/Material.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture2D.h"

namespace Engine {

    Material::Material(const std::shared_ptr<Shader>& shader)
        : m_Shader(shader) {
    }

    Material::~Material() = default; // <-- important

    void Material::SetTexture(uint32_t slot, const std::shared_ptr<Texture2D>& tex) {
        m_Textures[slot] = tex;
    }

} // namespace Engine