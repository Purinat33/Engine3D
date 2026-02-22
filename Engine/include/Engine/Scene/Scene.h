#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/UUID.h"

namespace Engine {

    class PerspectiveCamera;

    class Scene {
    public:
        Scene() = default;

        entt::registry& Registry() { return m_Registry; }
        const entt::registry& Registry() const { return m_Registry; }

        Entity CreateEntity(const char* name = "Entity");
        Entity CreateEntityWithUUID(UUID id, const char* name = "Entity");
        void DestroyEntity(Entity entity);

        void Clear();

        // ADD:
        Entity FindEntityByUUID(UUID id);
        Entity FindEntityByTag(const std::string& tag);

        void OnUpdate(float dt);
        void OnRender(const PerspectiveCamera& camera);

    private:
        entt::registry m_Registry;
    };

} // namespace Engine