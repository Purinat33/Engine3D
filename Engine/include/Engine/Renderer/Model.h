#pragma once
#include <string>
#include <vector>
#include <memory>

#include "Engine/Renderer/Mesh.h"

// Forward declares Assimp types
struct aiNode;
struct aiScene;
struct aiMesh;

namespace Engine {

    class Model {
    public:
        explicit Model(const std::string& path);

        const std::vector<std::shared_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }

    private:
        void LoadModel(const std::string& path);
        void ProcessNode(aiNode* node, const aiScene* scene);
        std::shared_ptr<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);

    private:
        std::vector<std::shared_ptr<Mesh>> m_Meshes;
        std::string m_Directory;
    };

} // namespace Engine