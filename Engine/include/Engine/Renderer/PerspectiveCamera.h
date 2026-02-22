#pragma once
#include <glm/glm.hpp>

namespace Engine {

    class PerspectiveCamera {
    public:
        PerspectiveCamera(float fovRadians, float aspect, float nearClip, float farClip);

        void SetViewportSize(float width, float height);

        void SetPosition(const glm::vec3& pos) { m_Position = pos; RecalculateView(); }
        void SetRotation(float yawRadians, float pitchRadians);

        const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

    private:
        void RecalculateProjection();
        void RecalculateView();

    private:
        float m_Fov = 1.0472f; // ~60deg
        float m_Aspect = 16.0f / 9.0f;
        float m_Near = 0.1f;
        float m_Far = 100.0f;

        glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
        float m_Yaw = 0.0f;   // radians
        float m_Pitch = 0.0f; // radians

        glm::mat4 m_Projection{ 1.0f };
        glm::mat4 m_View{ 1.0f };
        glm::mat4 m_ViewProjection{ 1.0f };
    };

} // namespace Engine