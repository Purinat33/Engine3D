#pragma once
#include <string>
#include <cstdint>

namespace Engine {

    class Shader {
    public:
        Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
        ~Shader();

        void Bind() const;

        uint32_t GetRendererID() const { return m_RendererID; }

        // value4x4 points to 16 floats (glm::value_ptr(mat))
        void SetMat4(const std::string& name, const float* value4x4) const;

    private:
        uint32_t CompileStage(uint32_t type, const std::string& src);
        uint32_t m_RendererID = 0;
    };

} // namespace Engine