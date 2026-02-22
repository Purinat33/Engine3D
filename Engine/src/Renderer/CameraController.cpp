#include "pch.h"
#include "Engine/Renderer/CameraController.h"
#include "Engine/Core/Input.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Engine {

    static glm::vec3 ForwardFromYawPitch(float yaw, float pitch) {
        // matches your camera fix: -Z forward when yaw=pitch=0
        glm::vec3 f{
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            -cosf(pitch) * cosf(yaw)
        };
        return glm::normalize(f);
    }

    CameraController::CameraController(float fovRadians, float aspect, float nearClip, float farClip)
        : m_Camera(fovRadians, aspect, nearClip, farClip) {
        m_Camera.SetPosition(m_Position);
        m_Camera.SetRotation(m_Yaw, m_Pitch);
    }

    void CameraController::OnUpdate(float dt) {
        // Mouse look
        float mx, my;
        Input::GetMousePosition(mx, my);

        if (m_FirstMouse) {
            m_LastMouseX = mx;
            m_LastMouseY = my;
            m_FirstMouse = false;
        }

        float dx = mx - m_LastMouseX;
        float dy = my - m_LastMouseY;
        m_LastMouseX = mx;
        m_LastMouseY = my;

        m_Yaw += dx * m_MouseSensitivity;
        m_Pitch -= dy * m_MouseSensitivity; // invert Y

        // Movement
        float speed = m_MoveSpeed;
        if (Input::IsKeyDown(KeyCode::LeftShift))
            speed *= 2.0f;

        glm::vec3 forward = ForwardFromYawPitch(m_Yaw, m_Pitch);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

        if (Input::IsKeyDown(KeyCode::W)) m_Position += forward * speed * dt;
        if (Input::IsKeyDown(KeyCode::S)) m_Position -= forward * speed * dt;
        if (Input::IsKeyDown(KeyCode::A)) m_Position -= right * speed * dt;
        if (Input::IsKeyDown(KeyCode::D)) m_Position += right * speed * dt;
        if (Input::IsKeyDown(KeyCode::Q)) m_Position.y -= speed * dt;
        if (Input::IsKeyDown(KeyCode::E)) m_Position.y += speed * dt;

        m_Camera.SetPosition(m_Position);
        m_Camera.SetRotation(m_Yaw, m_Pitch);
    }

} // namespace Engine