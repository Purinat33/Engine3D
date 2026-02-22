#pragma once
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Assets/AssetHandle.h"

namespace Engine {

    // Simple name/tag
    struct TagComponent {
        std::string Tag;
        TagComponent() = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    // Basic transform (Euler rotation in radians)
    struct TransformComponent {
        glm::vec3 Translation{ 0.0f };
        glm::vec3 Rotation{ 0.0f }; // pitch(x), yaw(y), roll(z) in radians
        glm::vec3 Scale{ 1.0f };

        glm::mat4 GetTransform() const {
            glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), Rotation.x, glm::vec3(1, 0, 0));
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), Rotation.y, glm::vec3(0, 1, 0));
            glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), Rotation.z, glm::vec3(0, 0, 1));
            glm::mat4 rot = rotZ * rotY * rotX;

            return glm::translate(glm::mat4(1.0f), Translation) * rot * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    // Renderable component: references a model asset by handle
    struct MeshRendererComponent {
        AssetHandle Model = InvalidAssetHandle;

        MeshRendererComponent() = default;
        MeshRendererComponent(AssetHandle modelHandle) : Model(modelHandle) {}
    };

    // One directional light is enough for now
    struct DirectionalLightComponent {
        glm::vec3 Direction{ 0.4f, 0.8f, -0.3f }; // points toward surfaces
        glm::vec3 Color{ 1.0f };

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const glm::vec3& dir, const glm::vec3& color)
            : Direction(dir), Color(color) {
        }
    };

} // namespace Engine