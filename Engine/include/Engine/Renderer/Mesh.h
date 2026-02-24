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

    struct Bounds {
        glm::vec3 Min{ 0.0f };
        glm::vec3 Max{ 0.0f };
        glm::vec3 Center{ 0.0f };
        float Radius = 0.0f; // bounding sphere radius in local space
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

        const std::shared_ptr<VertexArray>& GetVertexArray() const { return m_VAO; }
        uint32_t GetIndexCount() const;

        const Bounds& GetBounds() const { return m_Bounds; }

    private:
        std::shared_ptr<VertexArray> m_VAO;
        std::shared_ptr<IndexBuffer> m_IB;
        Bounds m_Bounds;
    };

} // namespace Engine