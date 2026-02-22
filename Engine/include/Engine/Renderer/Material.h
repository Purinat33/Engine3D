#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>

namespace Engine {

    class Shader;
    class Texture2D;

    class Material {
    public:
        explicit Material(const std::shared_ptr<Shader>& shader);
        ~Material(); // <-- add this

        const std::shared_ptr<Shader>& GetShader() const { return m_Shader; }

        void SetColor(const glm::vec4& color) { m_Color = color; m_HasColor = true; }
        bool HasColor() const { return m_HasColor; }
        const glm::vec4& GetColor() const { return m_Color; }

        void SetTexture(uint32_t slot, const std::shared_ptr<Texture2D>& tex);
        const std::unordered_map<uint32_t, std::shared_ptr<Texture2D>>& GetTextures() const { return m_Textures; }

    private:
        std::shared_ptr<Shader> m_Shader;

        bool m_HasColor = false;
        glm::vec4 m_Color{ 1.0f };

        std::unordered_map<uint32_t, std::shared_ptr<Texture2D>> m_Textures;
    };

} // namespace Engine