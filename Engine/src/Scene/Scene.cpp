#include "pch.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"

#include "Engine/Renderer/Renderer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <unordered_set>

namespace Engine {

    Entity Scene::CreateEntity(const char* name) {
        entt::entity e = m_Registry.create();

        Entity entity(e, &m_Registry);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>(name ? name : "Entity");
        return entity;
    }

    void Scene::DestroyEntity(Entity entity) {
        if (!entity) return;
        m_Registry.destroy(entity.GetHandle());
    }

    void Scene::OnUpdate(float /*dt*/) {
        // Later: scripts, animation, physics, etc.
    }

    void Scene::OnRender(const PerspectiveCamera& camera) {
        glm::vec3 lightDir{ 0.4f, 0.8f, -0.3f };
        glm::vec3 lightColor{ 1.0f };

        // Grab first directional light, if any
        {
            auto lightView = m_Registry.view<DirectionalLightComponent>();
            bool foundLight = false;

            lightView.each([&](auto /*entity*/, DirectionalLightComponent& light) {
                if (foundLight) return;
                lightDir = light.Direction;
                lightColor = light.Color;
                foundLight = true;
                });
        }

        // Collect unique shaders used by renderables
        std::unordered_set<Shader*> uniqueShaders;

        auto renderView = m_Registry.view<TransformComponent, MeshRendererComponent>();

        renderView.each([&](auto /*entity*/, TransformComponent& /*tc*/, MeshRendererComponent& mrc) {
            if (!mrc.ModelPtr) return;

            for (const auto& sm : mrc.ModelPtr->GetSubMeshes()) {
                if (!sm.MaterialPtr) continue;
                const auto& shader = sm.MaterialPtr->GetShader();
                if (shader) uniqueShaders.insert(shader.get());
            }
            });

        // Set light uniforms once per shader
        for (Shader* sh : uniqueShaders) {
            sh->Bind();
            sh->SetFloat3("u_LightDir", lightDir.x, lightDir.y, lightDir.z);
            sh->SetFloat3("u_LightColor", lightColor.x, lightColor.y, lightColor.z);
        }

        // Submit renderables
        Renderer::BeginScene(camera);

        renderView.each([&](auto /*entity*/, TransformComponent& tc, MeshRendererComponent& mrc) {
            if (!mrc.ModelPtr) return;

            glm::mat4 entityTransform = tc.GetTransform();

            for (const auto& sm : mrc.ModelPtr->GetSubMeshes()) {
                if (!sm.MeshPtr || !sm.MaterialPtr) continue;
                Renderer::Submit(sm.MaterialPtr, sm.MeshPtr->GetVertexArray(), entityTransform);
            }
            });

        Renderer::EndScene();
    }

} // namespace Engine