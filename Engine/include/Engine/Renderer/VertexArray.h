#pragma once
#include <memory>
#include <vector>
#include "Engine/Renderer/Buffer.h"

namespace Engine {

    class VertexArray {
    public:
        VertexArray();
        ~VertexArray();

        void Bind() const;

        void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vb);
        void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& ib);

        const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
        uint32_t GetRendererID() const { return m_RendererID; }

    private:
        uint32_t m_RendererID = 0;

        // KEEP BUFFERS ALIVE:
        std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
        std::shared_ptr<IndexBuffer> m_IndexBuffer;

        uint32_t m_AttribIndex = 0;
    };

} // namespace Engine