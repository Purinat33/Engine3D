#include "pch.h"
#include "Engine/Renderer/Shader.h"

#include <glad/glad.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

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

    std::string Shader::ReadFile(const std::string& filepath) {
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (!in) throw std::runtime_error("Failed to open shader file: " + filepath);

        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void Shader::ParseShaderFile(const std::string& source, std::string& outVertex, std::string& outFragment) {
        const std::string typeToken = "#type";
        size_t pos = 0;

        while ((pos = source.find(typeToken, pos)) != std::string::npos) {
            size_t eol = source.find_first_of("\r\n", pos);
            if (eol == std::string::npos) throw std::runtime_error("Shader parse error: missing EOL after #type");

            size_t begin = pos + typeToken.size();
            while (begin < eol && source[begin] == ' ') begin++;
            std::string type = source.substr(begin, eol - begin);

            size_t nextLine = source.find_first_not_of("\r\n", eol);
            if (nextLine == std::string::npos) throw std::runtime_error("Shader parse error: missing shader body");

            size_t nextType = source.find(typeToken, nextLine);
            std::string body = source.substr(nextLine, nextType - nextLine);

            if (type == "vertex") outVertex = body;
            else if (type == "fragment") outFragment = body;
            else throw std::runtime_error("Shader parse error: unknown type '" + type + "'");

            pos = nextType;
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

    uint32_t Shader::CreateProgram(const std::string& vertexSrc, const std::string& fragmentSrc) {
        uint32_t vs = CompileStage(GL_VERTEX_SHADER, vertexSrc);
        uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc);

        uint32_t program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        int ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            std::cerr << "Program link failed:\n";
            PrintShaderLog(program, true);
            glDeleteProgram(program);
            glDeleteShader(vs);
            glDeleteShader(fs);
            throw std::runtime_error("Program link failed");
        }

        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        return program;
    }

    Shader::Shader(const std::string& filepath) {
        m_Name = filepath;

        std::string src = ReadFile(filepath);
        std::string vertex, fragment;
        ParseShaderFile(src, vertex, fragment);

        m_RendererID = CreateProgram(vertex, fragment);
    }

    Shader::Shader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_Name(name) {
        m_RendererID = CreateProgram(vertexSrc, fragmentSrc);
    }

    Shader::~Shader() {
        glDeleteProgram(m_RendererID);
    }

    void Shader::Bind() const {
        glUseProgram(m_RendererID);
    }

    void Shader::SetMat4(const std::string& name, const float* value4x4) const {
        int loc = glGetUniformLocation(m_RendererID, name.c_str());
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, value4x4);
    }

    void Shader::SetFloat4(const std::string& name, float x, float y, float z, float w) const {
        int loc = glGetUniformLocation(m_RendererID, name.c_str());
        if (loc != -1) glUniform4f(loc, x, y, z, w);
    }

} // namespace Engine