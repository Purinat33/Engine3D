#pragma once
#include <glm/glm.hpp>
#include "Engine/Renderer/PerspectiveCamera.h"

namespace Engine {

    class CameraController {
    public:
        
        

        CameraController(float fovRadians, float aspect, float nearClip, float farClip);


        void OnUpdate(float dt);

        PerspectiveCamera& GetCamera() { return m_Camera; }
        const PerspectiveCamera& GetCamera() const { return m_Camera; }

        void SetMoveSpeed(float s) { m_MoveSpeed = s; }
        void SetMouseSensitivity(float s) { m_MouseSensitivity = s; }
        void SetActive(bool active) { m_Active = active; }
        bool IsActive() const { return m_Active; }
        glm::vec3 GetPosition() const { return m_Position; }
        glm::vec3 GetForward() const;
        glm::vec3 GetRight() const;

    private:
        PerspectiveCamera m_Camera;
        bool m_Active = true;

        float m_MoveSpeed = 4.5f;
        float m_MouseSensitivity = 0.0020f;

        float m_Yaw = 0.0f;
        float m_Pitch = 0.0f;

        bool m_FirstMouse = true;
        float m_LastMouseX = 0.0f;
        float m_LastMouseY = 0.0f;

        glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };
    };

} // namespace Engine