#include "pch.h"
#include "Engine/Renderer/Shader.h"

#include <glad/glad.h>
#include <vector>
#include <iostream>

namespace Engine {

    static void PrintShaderLog(uint32_t id, bool program) {
        int len = 0;
        if (program) glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
        else glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);

        if (len > 1) {
            std::vector<char> log((size_t)len);
            if (program) glGetProgramInfoLog(id, len, nullptr, log.data());
            else glGetShaderInfoLog(id, len, nullptr, log.data());
            std::cerr << log.data() << "\n";
        }
    }

    uint32_t Shader::CompileStage(uint32_t type, const std::string& src) {
        uint32_t id = glCreateShader(type);
        const char* cstr = src.c_str();
        glShaderSource(id, 1, &cstr, nullptr);
        glCompileShader(id);

        int ok = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            std::cerr << "Shader compile failed:\n";
            PrintShaderLog(id, false);
            glDeleteShader(id);
            throw std::runtime_error("Shader compile failed");
        }
        return id;
    }

    Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
        uint32_t vs = CompileStage(GL_VERTEX_SHADER, vertexSrc);
        uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc);

        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vs);
        glAttachShader(m_RendererID, fs);
        glLinkProgram(m_RendererID);

        int ok = 0;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &ok);
        if (!ok) {
            std::cerr << "Program link failed:\n";
            PrintShaderLog(m_RendererID, true);
            glDeleteProgram(m_RendererID);
            glDeleteShader(vs);
            glDeleteShader(fs);
            throw std::runtime_error("Program link failed");
        }

        glDetachShader(m_RendererID, vs);
        glDetachShader(m_RendererID, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    Shader::~Shader() {
        glDeleteProgram(m_RendererID);
    }

    void Shader::Bind() const {
        glUseProgram(m_RendererID);
    }

} // namespace Engine