#include "pch.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Shader.h"

namespace Engine {

    void ShaderLibrary::Add(const std::shared_ptr<Shader>& shader) {
        m_Shaders[shader->GetName()] = shader;
    }

    std::shared_ptr<Shader> ShaderLibrary::Load(const std::string& filepath) {
        auto shader = std::make_shared<Shader>(filepath);
        Add(shader);
        return shader;
    }

    std::shared_ptr<Shader> ShaderLibrary::Get(const std::string& name) const {
        auto it = m_Shaders.find(name);
        if (it == m_Shaders.end())
            throw std::runtime_error("Shader not found: " + name);
        return it->second;
    }

} // namespace Engine