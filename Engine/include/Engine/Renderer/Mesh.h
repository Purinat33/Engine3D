#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Engine {

    class VertexArray;
    class VertexBuffer;
    class IndexBuffer;

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

        const std::shared_ptr<VertexArray>& GetVertexArray() const { return m_VAO; }
        uint32_t GetIndexCount() const;

    private:
        std::shared_ptr<VertexArray> m_VAO;
        std::shared_ptr<IndexBuffer> m_IB;
    };

} // namespace Engine