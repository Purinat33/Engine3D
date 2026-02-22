#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace Engine {

    class Shader;
    class VertexArray;
    class PerspectiveCamera;
    class Texture2D;
    class Material;

    class Renderer {


    public:
        static void Init();

        static void BeginScene(const PerspectiveCamera& camera);
        static void Submit(const std::shared_ptr<Material>& material,
            const std::shared_ptr<VertexArray>& vao,
            const glm::mat4& model);
        static void EndScene(); // sorts + flushes

    private:
        

        struct DrawCommand {
            uint64_t SortKey = 0;
            std::shared_ptr<Material> MaterialPtr;
            std::shared_ptr<VertexArray> VaoPtr;
            glm::mat4 Model{ 1.0f };
        };

        static void Flush();

    private:
        static glm::mat4 s_ViewProjection;
        static std::vector<DrawCommand> s_DrawList;
    };

} // namespace Engine