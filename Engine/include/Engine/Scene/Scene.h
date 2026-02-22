#pragma once
#include <entt/entt.hpp>
#include "Engine/Scene/Entity.h"

namespace Engine {

    class PerspectiveCamera;

    class Scene {
    public:
        Scene() = default;

        Entity CreateEntity(const char* name = "Entity");
        void DestroyEntity(Entity entity);

        void OnUpdate(float dt);
        void OnRender(const PerspectiveCamera& camera);

    private:
        entt::registry m_Registry;
    };

} // namespace Engine