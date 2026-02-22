#include "pch.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/UUID.h"

#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <unordered_set>

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

        glm::vec3 lightDir{ 0.4f, 0.8f, -0.3f };
        glm::vec3 lightColor{ 1.0f };

        // First directional light
        {
            auto lightView = m_Registry.view<DirectionalLightComponent>();
            bool found = false;
            lightView.each([&](auto /*entity*/, DirectionalLightComponent& light) {
                if (found) return;
                lightDir = light.Direction;
                lightColor = light.Color;
                found = true;
                });
        }

        // Unique shaders used this frame
        std::unordered_set<Shader*> uniqueShaders;

        auto renderView = m_Registry.view<TransformComponent, MeshRendererComponent>();
        renderView.each([&](auto /*entity*/, TransformComponent&, MeshRendererComponent& mrc) {
            if (mrc.Model == InvalidAssetHandle) return;
            auto model = assets.GetModel(mrc.Model);
            if (!model) return;

            for (const auto& sm : model->GetSubMeshes()) {
                if (!sm.MaterialPtr) continue;
                const auto& shader = sm.MaterialPtr->GetShader();
                if (shader) uniqueShaders.insert(shader.get());
            }
            });

        for (Shader* sh : uniqueShaders) {
            sh->Bind();
            sh->SetFloat3("u_LightDir", lightDir.x, lightDir.y, lightDir.z);
            sh->SetFloat3("u_LightColor", lightColor.x, lightColor.y, lightColor.z);
        }

        /*Renderer::BeginScene(camera);*/

        renderView.each([&](auto /*entity*/, TransformComponent& tc, MeshRendererComponent& mrc) {
            if (mrc.Model == InvalidAssetHandle) return;

            auto model = assets.GetModel(mrc.Model);
            if (!model) return;

            glm::mat4 entityTransform = tc.GetTransform();

            for (const auto& sm : model->GetSubMeshes()) {
                if (!sm.MeshPtr || !sm.MaterialPtr) continue;
                Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), entityTransform);
            }
            });

        /*Renderer::EndScene();*/
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

    static uint32_t ToPickID(Engine::UUID id) {
        // fold 64 -> 32, keep 0 reserved as "none"
        uint32_t v = (uint32_t)(id ^ (id >> 32));
        return v == 0 ? 1u : v;
    }

    void Scene::OnRenderPicking(const PerspectiveCamera& camera, const std::shared_ptr<Material>& idMaterial) {
        (void)camera; // pipeline owns BeginScene(camera)

        auto& assets = AssetManager::Get();

        // We need ID + Transform + Mesh
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

} // namespace Engine