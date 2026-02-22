#include "pch.h"
#include "Engine/Renderer/PerspectiveCamera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Engine {

    PerspectiveCamera::PerspectiveCamera(float fovRadians, float aspect, float nearClip, float farClip)
        : m_Fov(fovRadians), m_Aspect(aspect), m_Near(nearClip), m_Far(farClip) {
        RecalculateProjection();
        RecalculateView();
    }

    void PerspectiveCamera::SetViewportSize(float width, float height) {
        m_Aspect = (height > 0.0f) ? (width / height) : m_Aspect;
        RecalculateProjection();
        m_ViewProjection = m_Projection * m_View;
    }

    void PerspectiveCamera::SetRotation(float yawRadians, float pitchRadians) {
        m_Yaw = yawRadians;
        // clamp pitch to avoid flipping
        const float limit = 1.5533f; // ~89deg
        m_Pitch = std::clamp(pitchRadians, -limit, limit);
        RecalculateView();
    }

    void PerspectiveCamera::RecalculateProjection() {
        m_Projection = glm::perspective(m_Fov, m_Aspect, m_Near, m_Far);
    }

    void PerspectiveCamera::RecalculateView() {
        // Forward direction from yaw/pitch
        glm::vec3 forward{
            cosf(m_Pitch) * sinf(m_Yaw),
            sinf(m_Pitch),
            -cosf(m_Pitch) * cosf(m_Yaw)
        };
        forward = glm::normalize(forward);

        m_View = glm::lookAt(m_Position, m_Position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        m_ViewProjection = m_Projection * m_View;
    }

} // namespace Engine