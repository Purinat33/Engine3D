#include "pch.h"
#include "Engine/Renderer/Mesh.h"

#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"

#include <cstddef>

namespace Engine {

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        m_VAO = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(vertices.data(),
            (uint32_t)(vertices.size() * sizeof(Vertex)));

        vb->SetLayout(BufferLayout({
            { ShaderDataType::Float3, (uint32_t)offsetof(Vertex, Position) },
            { ShaderDataType::Float3, (uint32_t)offsetof(Vertex, Normal) },
            { ShaderDataType::Float2, (uint32_t)offsetof(Vertex, TexCoord) }
            }, (uint32_t)sizeof(Vertex)));

        m_VAO->AddVertexBuffer(vb);

        m_IB = std::make_shared<IndexBuffer>(indices.data(), (uint32_t)indices.size());
        m_VAO->SetIndexBuffer(m_IB);

        {
            glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);

            for (const auto& v : vertices) {
                mn = glm::min(mn, v.Position);
                mx = glm::max(mx, v.Position);
            }

            m_Bounds.Min = mn;
            m_Bounds.Max = mx;
            m_Bounds.Center = (mn + mx) * 0.5f;

            float r2 = 0.0f;
            for (const auto& v : vertices) {
                glm::vec3 d = v.Position - m_Bounds.Center;
                r2 = std::max(r2, glm::dot(d, d));
            }
            m_Bounds.Radius = std::sqrt(r2);
        }

    }

    uint32_t Mesh::GetIndexCount() const {
        return m_IB ? m_IB->GetCount() : 0;
    }

    static_assert(sizeof(Vertex) == 32, "Vertex struct size is not 32 bytes; stride mismatch likely!");



} // namespace Engine