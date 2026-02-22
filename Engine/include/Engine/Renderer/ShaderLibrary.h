#pragma once
#include <unordered_map>
#include <memory>
#include <string>

namespace Engine {

    class Shader;

    class ShaderLibrary {
    public:
        void Add(const std::shared_ptr<Shader>& shader);
        std::shared_ptr<Shader> Load(const std::string& filepath);
        std::shared_ptr<Shader> Get(const std::string& name) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
    };

} // namespace Engine