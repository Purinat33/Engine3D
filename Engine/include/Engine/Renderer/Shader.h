#pragma once
#include <string>
#include <cstdint>

namespace Engine {

    class Shader {
    public:
        // From file (our #type format)
        explicit Shader(const std::string& filepath);

        void SetUInt(const std::string& name, uint32_t value) const;

        // From source strings (keep for quick tests)
        Shader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);

        ~Shader();

        const std::string& GetName() const { return m_Name; }
        uint32_t GetRendererID() const { return m_RendererID; }

        void Bind() const;

        void SetMat4(const std::string& name, const float* value4x4) const;
        void SetFloat4(const std::string& name, float x, float y, float z, float w) const;

        void SetInt(const std::string& name, int value) const;

        void SetFloat3(const std::string& name, float x, float y, float z) const;

    private:
        uint32_t CompileStage(uint32_t type, const std::string& src);
        uint32_t CreateProgram(const std::string& vertexSrc, const std::string& fragmentSrc);

        std::string ReadFile(const std::string& filepath);
        void ParseShaderFile(const std::string& source, std::string& outVertex, std::string& outFragment);

    private:
        uint32_t m_RendererID = 0;
        std::string m_Name;
    };

} // namespace Engine