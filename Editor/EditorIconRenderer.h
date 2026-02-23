#pragma once
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Engine {
    class Shader;
    class Material;
    class Texture2D;
    class VertexArray;
    class Scene;
    class CameraController;
}

class EditorIconRenderer {
public:
    void Init();
    void Draw(Engine::Scene& scene,
        Engine::CameraController& cam,
        uint32_t selectedPickID);

private:
    std::shared_ptr<Engine::Shader> m_Shader;
    std::shared_ptr<Engine::VertexArray> m_Quad;
    std::unordered_map<int, std::shared_ptr<Engine::Texture2D>> m_IconTex; // by EditorIconType int

    std::shared_ptr<Engine::Texture2D> GetIcon(int type);
};