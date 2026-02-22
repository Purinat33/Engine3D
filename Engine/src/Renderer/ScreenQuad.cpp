#include "pch.h"
#include "Engine/Renderer/ScreenQuad.h"

#include "Engine/Renderer/VertexArray.h"
#include "Engine/Renderer/Buffer.h"

namespace Engine {

    std::shared_ptr<VertexArray> ScreenQuad::GetVAO() {
        static std::shared_ptr<VertexArray> s_VAO;

        if (s_VAO) return s_VAO;

        // pos (x,y) + uv (u,v)
        float vertices[] = {
            -1.f, -1.f,  0.f, 0.f,
             1.f, -1.f,  1.f, 0.f,
             1.f,  1.f,  1.f, 1.f,
            -1.f,  1.f,  0.f, 1.f
        };
        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        s_VAO = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(vertices, sizeof(vertices));
        vb->SetLayout({
            { ShaderDataType::Float2 }, // pos
            { ShaderDataType::Float2 }  // uv
            });
        s_VAO->AddVertexBuffer(vb);

        auto ib = std::make_shared<IndexBuffer>(indices, 6);
        s_VAO->SetIndexBuffer(ib);

        return s_VAO;
    }

} // namespace Engine