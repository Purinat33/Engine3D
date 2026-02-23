#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace Engine {

    class Shader;
    class VertexArray;
    class PerspectiveCamera;
    class Material;

    class Renderer {
    public:
        static void Init();

        static void BeginScene(const PerspectiveCamera& camera);

        // Normal submit (no picking ID)
        static void Submit(const std::shared_ptr<Material>& material,
            const std::shared_ptr<VertexArray>& vao,
            const glm::mat4& model);

        // Picking submit (writes u_EntityID)
        static void Submit(const std::shared_ptr<Material>& material,
            const std::shared_ptr<VertexArray>& vao,
            const glm::mat4& model,
            uint32_t entityID);

        static void EndScene();

        // --- Lighting (simple global directional light for now) ---
        static void SetDirectionalLight(const glm::vec3& dir, const glm::vec3& color);
        static void ClearLights();

        static bool s_HasDirLight;
        static glm::vec3 s_DirLightDir;
        static glm::vec3 s_DirLightColor;

    private:
        struct DrawCommand {
            uint64_t SortKey = 0;
            std::shared_ptr<Material> MaterialPtr;
            std::shared_ptr<VertexArray> VaoPtr;
            glm::mat4 Model{ 1.0f };
            uint32_t EntityID = 0;
        };

        static void Flush();

    private:
        static glm::mat4 s_ViewProjection;
        static std::vector<DrawCommand> s_DrawList;
    };

} // namespace Engine