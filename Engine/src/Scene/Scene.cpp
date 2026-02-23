#include "pch.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/UUID.h"

#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <cmath>

namespace Engine {

    Entity Scene::CreateEntity(const char* name) {
        return CreateEntityWithUUID(GenerateUUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID id, const char* name) {
        entt::entity e = m_Registry.create();
        Entity entity(e, &m_Registry);

        entity.AddComponent<IDComponent>(id);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>(name ? name : "Entity");
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        if (!entity) return;
        m_Registry.destroy(entity.GetHandle());
    }

    void Scene::Clear() {
        m_Registry.clear();
    }

    void Scene::OnUpdate(float /*dt*/) {
        // Later: scripts, animation, physics, etc.
    }

    void Scene::OnRender(const PerspectiveCamera& camera) {
        auto& assets = AssetManager::Get();

        // --- Lighting: pick first directional light, or use a default ---
        Renderer::ClearLights();

        bool foundLight = false;
        auto lightView = m_Registry.view<TransformComponent, DirectionalLightComponent>();
        for (auto e : lightView) {
            auto& tc = lightView.get<TransformComponent>(e);
            auto& dl = lightView.get<DirectionalLightComponent>(e);

            // Transform rotation -> forward dir (-Z baseline)
            glm::vec3 dir{
                cosf(tc.Rotation.x) * sinf(tc.Rotation.y),
                sinf(tc.Rotation.x),
                -cosf(tc.Rotation.x) * cosf(tc.Rotation.y)
            };

            if (glm::length(dir) < 0.0001f)
                dir = dl.Direction;

            Renderer::SetDirectionalLight(glm::normalize(dir), dl.Color);

            // Optional: keep component Direction in sync with what rotation means
            dl.Direction = glm::normalize(dir);

            foundLight = true;
            break;
        }

        if (!foundLight) {
            // Keep lighting mode "on" so you don't get the confusing unlit-albedo look.
            Renderer::SetDirectionalLight(glm::vec3(0.4f, 0.8f, -0.3f), glm::vec3(1.0f));
        }

        // --- Render meshes (submit only; pipeline owns BeginScene/EndScene) ---
        auto renderView = m_Registry.view<TransformComponent, MeshRendererComponent>();
        renderView.each([&](auto /*entity*/, TransformComponent& tc, MeshRendererComponent& mrc) {
            if (mrc.Model == InvalidAssetHandle) return;

            auto model = assets.GetModel(mrc.Model);
            if (!model) return;

            glm::mat4 world = tc.GetTransform();

            for (const auto& sm : model->GetSubMeshes()) {
                if (!sm.MeshPtr || !sm.MaterialPtr) continue;
                Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), world);
            }
            });
    }

    Entity Scene::FindEntityByUUID(UUID id) {
        auto view = m_Registry.view<IDComponent>();
        for (auto e : view) {
            const auto& idc = view.get<IDComponent>(e);
            if (idc.ID == id)
                return Entity(e, &m_Registry);
        }
        return {};
    }

    Entity Scene::FindEntityByTag(const std::string& tag) {
        auto view = m_Registry.view<TagComponent>();
        for (auto e : view) {
            const auto& tc = view.get<TagComponent>(e);
            if (tc.Tag == tag)
                return Entity(e, &m_Registry);
        }
        return {};
    }

    Entity Scene::FindEntityByPickID(uint32_t pickID) {
        if (pickID == 0) return {};

        auto view = m_Registry.view<IDComponent>();
        for (auto e : view) {
            const auto& idc = view.get<IDComponent>(e);
            uint32_t folded = (uint32_t)(idc.ID ^ (idc.ID >> 32));
            if (folded == 0) folded = 1u;
            if (folded == pickID)
                return Entity(e, &m_Registry);
        }
        return {};
    }

    static uint32_t ToPickID(Engine::UUID id) {
        uint32_t v = (uint32_t)(id ^ (id >> 32));
        return v == 0 ? 1u : v;
    }

    void Scene::OnRenderPicking(const PerspectiveCamera& /*camera*/, const std::shared_ptr<Material>& idMaterial) {
        auto& assets = AssetManager::Get();

        auto view = m_Registry.view<IDComponent, TransformComponent, MeshRendererComponent>();
        view.each([&](auto /*entity*/, IDComponent& idc, TransformComponent& tc, MeshRendererComponent& mrc) {
            if (mrc.Model == InvalidAssetHandle) return;

            auto model = assets.GetModel(mrc.Model);
            if (!model) return;

            uint32_t pickID = ToPickID(idc.ID);
            glm::mat4 world = tc.GetTransform();

            for (const auto& sm : model->GetSubMeshes()) {
                if (!sm.MeshPtr) continue;
                Renderer::Submit(idMaterial, sm.MeshPtr->GetVertexArray(), world, pickID);
            }
            });
    }

    Entity Scene::DuplicateEntity(Entity src) {
        if (!src) return {};

        std::string name = "Entity Copy";
        if (src.HasComponent<TagComponent>())
            name = src.GetComponent<TagComponent>().Tag + " Copy";

        Entity dst = CreateEntity(name.c_str());

        if (src.HasComponent<TransformComponent>())
            dst.GetComponent<TransformComponent>() = src.GetComponent<TransformComponent>();

        if (src.HasComponent<MeshRendererComponent>())
            dst.AddComponent<MeshRendererComponent>(src.GetComponent<MeshRendererComponent>());

        if (src.HasComponent<DirectionalLightComponent>())
            dst.AddComponent<DirectionalLightComponent>(src.GetComponent<DirectionalLightComponent>());

        if (src.HasComponent<SpawnPointComponent>())
            dst.AddComponent<SpawnPointComponent>(src.GetComponent<SpawnPointComponent>());

        if (src.HasComponent<SceneWarpComponent>())
            dst.AddComponent<SceneWarpComponent>(src.GetComponent<SceneWarpComponent>());

        return dst;
    }

} // namespace Engine