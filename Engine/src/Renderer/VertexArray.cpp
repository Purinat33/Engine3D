#include "pch.h"
#include "Engine/Renderer/VertexArray.h"

#include <glad/glad.h>

namespace Engine {

    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
            return GL_FLOAT;
        default:
            return GL_FLOAT;
        }
    }

    VertexArray::VertexArray() {
        glGenVertexArrays(1, &m_RendererID);
    }

    VertexArray::~VertexArray() {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void VertexArray::Bind() const {
        glBindVertexArray(m_RendererID);
    }

    void VertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vb) {
        Bind();
        vb->Bind();

        const auto& layout = vb->GetLayout();
        for (const auto& element : layout.GetElements()) {
            glEnableVertexAttribArray(m_AttribIndex);
            glVertexAttribPointer(
                m_AttribIndex,
                element.ComponentCount(),
                ShaderDataTypeToOpenGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                layout.GetStride(),
                (const void*)(uintptr_t)element.Offset
            );
            m_AttribIndex++;
        }
    }

    void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& ib) {
        Bind();
        ib->Bind();
        m_IndexBuffer = ib;
    }

} // namespace Engine