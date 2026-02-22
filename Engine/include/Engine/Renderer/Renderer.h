#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Engine {

    class Shader;
    class VertexArray;
    class PerspectiveCamera;
    class Texture2D;

    class Renderer {
    public:
        static void Init();

        static void BeginScene(const PerspectiveCamera& camera);
        static void Submit(const std::shared_ptr<Shader>& shader,
            const std::shared_ptr<VertexArray>& vao,
            const glm::mat4& model,
            const glm::vec4& color = { 1,1,1,1 },
            const std::shared_ptr<Texture2D>& texture = nullptr);
        static void EndScene(); // sorts + flushes

    private:
        struct DrawCommand {
            uint64_t SortKey = 0;
            std::shared_ptr<Shader> ShaderPtr;
            std::shared_ptr<VertexArray> VaoPtr;
            std::shared_ptr<Texture2D> Texture;
            glm::mat4 Model{ 1.0f };
            glm::vec4 Color{ 1.0f };
        };

        static void Flush();

    private:
        static glm::mat4 s_ViewProjection;
        static std::vector<DrawCommand> s_DrawList;
    };

} // namespace Engine