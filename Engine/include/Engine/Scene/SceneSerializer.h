#pragma once
#include <string>

namespace Engine {

    class Scene;

    class SceneSerializer {
    public:
        explicit SceneSerializer(Scene& scene);

        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        Scene& m_Scene;
    };

} // namespace Engine