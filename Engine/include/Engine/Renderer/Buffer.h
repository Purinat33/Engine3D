#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>

namespace Engine {

    enum class ShaderDataType {
        None = 0,
        Float, Float2, Float3, Float4
    };

    static uint32_t ShaderDataTypeSize(ShaderDataType type) {
        switch (type) {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 4 * 2;
        case ShaderDataType::Float3: return 4 * 3;
        case ShaderDataType::Float4: return 4 * 4;
        default: return 0;
        }
    }

    struct BufferElement {
        ShaderDataType Type = ShaderDataType::None;
        uint32_t Offset = 0;
        bool Normalized = false;

        BufferElement() = default;

        // automatic layout
        BufferElement(ShaderDataType type, bool normalized = false)
            : Type(type), Offset(0), Normalized(normalized) {
        }

        // explicit offset layout
        BufferElement(ShaderDataType type, uint32_t offset, bool normalized = false)
            : Type(type), Offset(offset), Normalized(normalized) {
        }

        uint32_t Size() const { return ShaderDataTypeSize(Type); }
        uint32_t ComponentCount() const {
            switch (Type) {
            case ShaderDataType::Float:  return 1;
            case ShaderDataType::Float2: return 2;
            case ShaderDataType::Float3: return 3;
            case ShaderDataType::Float4: return 4;
            default: return 0;
            }
        }
    };

    class BufferLayout {
    public:
        BufferLayout() = default;

        // auto offsets + stride = sum(sizes)
        BufferLayout(std::initializer_list<BufferElement> elements)
            : m_Elements(elements) {
            CalculateOffsetsAndStride();
        }

        // explicit offsets, explicit stride (for struct-based vertex layouts)
        BufferLayout(std::initializer_list<BufferElement> elements, uint32_t stride)
            : m_Elements(elements), m_Stride(stride) {
        }

        uint32_t GetStride() const { return m_Stride; }
        const std::vector<BufferElement>& GetElements() const { return m_Elements; }

    private:
        void CalculateOffsetsAndStride() {
            uint32_t offset = 0;
            m_Stride = 0;
            for (auto& e : m_Elements) {
                e.Offset = offset;
                offset += e.Size();
                m_Stride += e.Size();
            }
        }

    private:
        std::vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };


    class VertexBuffer {
    public:
        VertexBuffer(const void* data, uint32_t size);
        ~VertexBuffer();

        void Bind() const;

        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }
        const BufferLayout& GetLayout() const { return m_Layout; }

    private:
        uint32_t m_RendererID = 0;
        BufferLayout m_Layout;
    };

    class IndexBuffer {
    public:
        IndexBuffer(const uint32_t* indices, uint32_t count);
        ~IndexBuffer();

        void Bind() const;
        uint32_t GetCount() const { return m_Count; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Count = 0;
    };

} // namespace Engine