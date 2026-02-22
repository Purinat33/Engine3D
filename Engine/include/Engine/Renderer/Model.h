#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Engine/Renderer/Mesh.h"

struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

namespace Engine {

    class Shader;
    class Texture2D;
    class Material;

    class Model {
    public:
        // Pass the shader you want model materials to use
        Model(const std::string& path, const std::shared_ptr<Shader>& defaultShader);

        struct SubMesh {
            std::shared_ptr<Mesh> MeshPtr;
            std::shared_ptr<Material> MaterialPtr;
        };

        const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }

    private:
        void LoadModel(const std::string& path);
        void ProcessNode(aiNode* node, const aiScene* scene);
        SubMesh ProcessMesh(aiMesh* mesh, const aiScene* scene);

        std::shared_ptr<Texture2D> LoadTextureFromMaterial(aiMaterial* mat, const aiScene* scene);

    private:
        std::vector<SubMesh> m_SubMeshes;
        std::string m_Directory;

        std::shared_ptr<Shader> m_DefaultShader;

        // Cache textures by absolute path so duplicates aren’t reloaded
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_TextureCache;
    };

} // namespace Engine