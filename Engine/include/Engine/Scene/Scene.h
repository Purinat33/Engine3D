#pragma once
#include <entt/entt.hpp>
#include <string>
#include <memory>
#include <cstdint>
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/UUID.h"

#include <glm/glm.hpp>

namespace Engine {

    class PerspectiveCamera;
    class Material; // <-- ADD

    class Scene {
    public:
        Scene() = default;

        Entity FindEntityByPickID(uint32_t pickID);

        entt::registry& Registry() { return m_Registry; }
        const entt::registry& Registry() const { return m_Registry; }

        Entity CreateEntity(const char* name = "Entity");
        Entity CreateEntityWithUUID(UUID id, const char* name = "Entity");
        Entity DuplicateEntity(Entity src);
        void DestroyEntity(Entity entity);

        void Clear();

        Entity FindEntityByUUID(UUID id);
        Entity FindEntityByTag(const std::string& tag);

        void OnUpdate(float dt);
        void OnRender(const PerspectiveCamera& camera);

        void OnRenderPicking(const PerspectiveCamera& camera,
            const std::shared_ptr<Material>& idMaterial);
        
        void OnRenderShadow(const std::shared_ptr<Material>& shadowDepthMat);
        bool GetMainDirectionalLight(glm::vec3& outDir, glm::vec3& outColor);

    private:
        entt::registry m_Registry;
    };

} // namespace Engine