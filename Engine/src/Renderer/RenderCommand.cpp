#include "pch.h"
#include "Engine/Renderer/RenderCommand.h"

#include <glad/glad.h>

namespace Engine {

    void RenderCommand::Init() {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Optional but recommended for sane 3D
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Ensure not stuck in wireframe from previous tests
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        glViewport((int)x, (int)y, (int)width, (int)height);
    }

    void RenderCommand::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void RenderCommand::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void RenderCommand::DrawIndexed(uint32_t indexCount) {
        glDrawElements(GL_TRIANGLES, (int)indexCount, GL_UNSIGNED_INT, nullptr);
    }

} // namespace Engine