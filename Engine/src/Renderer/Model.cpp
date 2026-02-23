#include "pch.h"
#include "Engine/Renderer/Model.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Texture2D.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace Engine {

    static std::string JoinPathFS(const std::string& dir, const std::string& file) {
        if (dir.empty()) return file;
        std::filesystem::path p = std::filesystem::path(dir) / std::filesystem::path(file);
        return p.lexically_normal().string();
    }

    Model::Model(const std::string& path, const std::shared_ptr<Shader>& defaultShader)
        : m_DefaultShader(defaultShader) {
        LoadModel(path);
    }

    void Model::LoadModel(const std::string& path) {
        Assimp::Importer importer;

        // You can optionally add aiProcess_FlipUVs if textures appear upside-down for some assets.
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
        m_SourcePath = path; // NEW: keep for cache keys

        m_SubMeshes.clear();
        m_TextureCache.clear();
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

    // Loads baseColor/diffuse only for now (slot 0).
    std::shared_ptr<Texture2D> Model::LoadTextureFromMaterial(aiMaterial* mat, const aiScene* scene) {
        auto tryType = [&](aiTextureType type) -> std::string {
            if (!mat || mat->GetTextureCount(type) == 0) return {};
            aiString str;
            if (mat->GetTexture(type, 0, &str) != AI_SUCCESS) return {};
            return str.C_Str();
            };

        // glTF
        std::string texName = tryType(aiTextureType_BASE_COLOR);
        // other formats
        if (texName.empty())
            texName = tryType(aiTextureType_DIFFUSE);

        if (texName.empty())
            return nullptr;

        // Cache key
        std::string cacheKey = m_SourcePath + "|" + texName;
        if (auto it = m_TextureCache.find(cacheKey); it != m_TextureCache.end())
            return it->second;

        // ---- Embedded texture (GLB often uses this) ----
        if (scene) {
            if (const aiTexture* embedded = scene->GetEmbeddedTexture(texName.c_str())) {
                // Compressed image data (PNG/JPG) if mHeight == 0
                if (embedded->mHeight == 0) {
                    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(embedded->pcData);
                    size_t sizeBytes = (size_t)embedded->mWidth;

                    Texture2D* raw = Texture2D::CreateFromMemory(texName, bytes, sizeBytes);
                    auto tex = std::shared_ptr<Texture2D>(raw);

                    m_TextureCache[cacheKey] = tex;
                    std::cout << "[Model] Loaded embedded texture: " << texName << "\n";
                    return tex;
                }

                // Uncompressed (rare): aiTexel array (RGBA)
                {
                    int w = (int)embedded->mWidth;
                    int h = (int)embedded->mHeight;

                    std::vector<uint8_t> rgba;
                    rgba.resize((size_t)w * (size_t)h * 4);

                    for (int i = 0; i < w * h; i++) {
                        rgba[i * 4 + 0] = embedded->pcData[i].r;
                        rgba[i * 4 + 1] = embedded->pcData[i].g;
                        rgba[i * 4 + 2] = embedded->pcData[i].b;
                        rgba[i * 4 + 3] = embedded->pcData[i].a;
                    }

                    Texture2D* raw = Texture2D::CreateFromRGBA8(texName, rgba.data(), w, h);
                    auto tex = std::shared_ptr<Texture2D>(raw);

                    m_TextureCache[cacheKey] = tex;
                    std::cout << "[Model] Loaded embedded RGBA texture: " << texName << "\n";
                    return tex;
                }
            }
        }

        // ---- External texture (gltf+pngs, obj+mtl, etc.) ----
        std::string fullPath = JoinPathFS(m_Directory, texName);

        auto tex = std::shared_ptr<Texture2D>(new Texture2D(fullPath));
        m_TextureCache[cacheKey] = tex;

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