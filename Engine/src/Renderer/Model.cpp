#include "pch.h"
#include "Engine/Renderer/Model.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture2D.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

namespace Engine {

    static std::string JoinPath(const std::string& dir, const std::string& file) {
        if (dir.empty()) return file;
        char last = dir.back();
        if (last == '/' || last == '\\') return dir + file;
        return dir + "\\" + file;
    }

    Model::Model(const std::string& path, const std::shared_ptr<Shader>& defaultShader)
        : m_DefaultShader(defaultShader) {
        LoadModel(path);
    }

    void Model::LoadModel(const std::string& path) {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality
        );

        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
            throw std::runtime_error(std::string("Assimp failed: ") + importer.GetErrorString());
        }

        auto slash = path.find_last_of("/\\");
        m_Directory = (slash == std::string::npos) ? "" : path.substr(0, slash);

        m_SubMeshes.clear();
        ProcessNode(scene->mRootNode, scene);

        std::cout << "[Model] Loaded: " << path << " submeshes=" << m_SubMeshes.size() << "\n";
    }

    void Model::ProcessNode(aiNode* node, const aiScene* scene) {
        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            m_SubMeshes.push_back(ProcessMesh(mesh, scene));
        }

        for (unsigned i = 0; i < node->mNumChildren; i++)
            ProcessNode(node->mChildren[i], scene);
    }

    std::shared_ptr<Texture2D> Model::LoadTextureFromMaterial(aiMaterial* mat, const aiScene* scene) {
        // Prefer BaseColor (glTF) if available, else Diffuse (OBJ/FBX/etc.)
        auto tryType = [&](aiTextureType type) -> std::string {
            if (!mat || mat->GetTextureCount(type) == 0) return {};
            aiString str;
            if (mat->GetTexture(type, 0, &str) != AI_SUCCESS) return {};
            return str.C_Str();
            };

        std::string texRel = tryType(aiTextureType_BASE_COLOR);
        if (texRel.empty())
            texRel = tryType(aiTextureType_DIFFUSE);

        if (texRel.empty())
            return nullptr;

        // Embedded textures show up like "*0" — we skip those for now
        if (!texRel.empty() && texRel[0] == '*') {
            std::cout << "[Model] Embedded texture detected (" << texRel << "), not supported yet.\n";
            return nullptr;
        }

        std::string fullPath = JoinPath(m_Directory, texRel);

        // Cache
        auto it = m_TextureCache.find(fullPath);
        if (it != m_TextureCache.end())
            return it->second;

        auto tex = std::make_shared<Texture2D>(fullPath);
        m_TextureCache[fullPath] = tex;

        std::cout << "[Model] Loaded texture: " << fullPath << "\n";
        return tex;
    }

    Model::SubMesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        vertices.reserve(mesh->mNumVertices);

        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            Vertex v{};
            v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

            if (mesh->HasNormals())
                v.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            else
                v.Normal = { 0.0f, 1.0f, 0.0f };

            if (mesh->mTextureCoords[0])
                v.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            else
                v.TexCoord = { 0.0f, 0.0f };

            vertices.push_back(v);
        }

        for (unsigned i = 0; i < mesh->mNumFaces; i++) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        auto meshObj = std::make_shared<Mesh>(vertices, indices);

        // Build a material for this mesh
        auto matObj = std::make_shared<Material>(m_DefaultShader);
        matObj->SetColor({ 1, 1, 1, 1 });

        if (scene && mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* aiMat = scene->mMaterials[mesh->mMaterialIndex];
            auto tex0 = LoadTextureFromMaterial(aiMat, scene);
            if (tex0)
                matObj->SetTexture(0, tex0);
        }

        return { meshObj, matObj };
    }

} // namespace Engine