#include "pch.h"
#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/Texture2D.h"

#include <iostream>

namespace Engine {

    AssetManager& AssetManager::Get() {
        static AssetManager s_Instance;
        return s_Instance;
    }

    AssetManager::AssetManager()
        : m_Registry("Assets/Project/asset_registry.json") {
        m_Registry.Load();
    }

    AssetHandle AssetManager::LoadShader(const std::string& path) {
        // Register in registry
        AssetHandle id = m_Registry.Register(AssetType::Shader, path);
        m_Registry.Save();

        // Cached?
        if (m_ShaderCache.find(id) != m_ShaderCache.end())
            return id;

        // Load
        auto shader = std::make_shared<Shader>(path);
        m_ShaderCache[id] = shader;

        return id;
    }

    AssetHandle AssetManager::LoadModel(const std::string& path, AssetHandle shaderHandle) {
        // Make sure shader exists (and cached)
        auto shader = GetShader(shaderHandle);
        if (!shader) {
            std::cout << "[AssetManager] Missing shader handle for model: " << path << "\n";
            return InvalidAssetHandle;
        }

        AssetHandle id = m_Registry.Register(AssetType::Model, path, shaderHandle);
        m_Registry.Save();

        if (m_ModelCache.find(id) != m_ModelCache.end())
            return id;

        auto model = std::make_shared<Model>(m_Registry.Get(id)->Path, shader);
        m_ModelCache[id] = model;

        return id;
    }

    AssetHandle AssetManager::LoadTexture2D(const std::string& path) {
        AssetHandle id = m_Registry.Register(AssetType::Texture2D, path);
        m_Registry.Save();

        if (m_TextureCache.find(id) != m_TextureCache.end())
            return id;

        auto tex = std::make_shared<Texture2D>(m_Registry.Get(id)->Path);
        m_TextureCache[id] = tex;

        return id;
    }

    std::shared_ptr<Shader> AssetManager::GetShader(AssetHandle shaderHandle) {
        if (shaderHandle == InvalidAssetHandle) return nullptr;

        if (auto it = m_ShaderCache.find(shaderHandle); it != m_ShaderCache.end())
            return it->second;

        // Not in cache -> load from registry
        const AssetMetadata* meta = m_Registry.Get(shaderHandle);
        if (!meta || meta->Type != AssetType::Shader) return nullptr;

        auto shader = std::make_shared<Shader>(meta->Path);
        m_ShaderCache[shaderHandle] = shader;
        return shader;
    }

    std::shared_ptr<Model> AssetManager::GetModel(AssetHandle modelHandle) {
        if (modelHandle == InvalidAssetHandle) return nullptr;

        if (auto it = m_ModelCache.find(modelHandle); it != m_ModelCache.end())
            return it->second;

        const AssetMetadata* meta = m_Registry.Get(modelHandle);
        if (!meta || meta->Type != AssetType::Model) return nullptr;

        auto shader = GetShader(meta->Shader);
        if (!shader) {
            std::cout << "[AssetManager] Model has missing shader handle: " << meta->Path << "\n";
            return nullptr;
        }

        auto model = std::make_shared<Model>(meta->Path, shader);
        m_ModelCache[modelHandle] = model;
        return model;
    }

    std::shared_ptr<Texture2D> AssetManager::GetTexture2D(AssetHandle texHandle) {
        if (texHandle == InvalidAssetHandle) return nullptr;

        if (auto it = m_TextureCache.find(texHandle); it != m_TextureCache.end())
            return it->second;

        const AssetMetadata* meta = m_Registry.Get(texHandle);
        if (!meta || meta->Type != AssetType::Texture2D) return nullptr;

        auto tex = std::make_shared<Texture2D>(meta->Path);
        m_TextureCache[texHandle] = tex;
        return tex;
    }

    AssetManager::ModelInfo AssetManager::GetModelInfo(AssetHandle modelHandle) const {
        ModelInfo info{};
        const AssetMetadata* meta = m_Registry.Get(modelHandle);
        if (!meta || meta->Type != AssetType::Model) return info;
        info.Path = meta->Path;
        info.ShaderHandle = meta->Shader;
        return info;
    }

    std::string AssetManager::GetShaderPath(AssetHandle shaderHandle) const {
        const AssetMetadata* meta = m_Registry.Get(shaderHandle);
        if (!meta || meta->Type != AssetType::Shader) return {};
        return meta->Path;
    }

} // namespace Engine