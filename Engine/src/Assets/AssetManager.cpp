#include "pch.h"
#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/Texture2D.h"

#include "Engine/Core/Content.h"

#include <iostream>
#include <filesystem>

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
        std::string resolved = Content::Resolve(path);
        if (!Content::Exists(resolved)) {
            std::cout << "[AssetManager] Shader file missing: " << resolved << "\n";
            return InvalidAssetHandle;
        }

        // Register first ONLY after file exists
        AssetHandle id = m_Registry.Register(AssetType::Shader, resolved);
        m_Registry.Save();

        if (auto it = m_ShaderCache.find(id); it != m_ShaderCache.end())
            return id;

        try {
            auto shader = std::make_shared<Shader>(resolved);
            m_ShaderCache[id] = shader;
            return id;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Shader load failed: " << resolved << " (" << e.what() << ")\n";
            m_Registry.Remove(id);
            m_Registry.Save();
            return InvalidAssetHandle;
        }
    }
    
    AssetHandle AssetManager::LoadModel(const std::string& path, AssetHandle shaderHandle) {
        auto shader = GetShader(shaderHandle);
        if (!shader) {
            std::cout << "[AssetManager] Missing shader handle for model: " << path << "\n";
            return InvalidAssetHandle;
        }

        std::string resolved = Content::Resolve(path);
        if (!Content::Exists(resolved)) {
            std::cout << "[AssetManager] Model file missing: " << resolved << "\n";
            return InvalidAssetHandle;
        }

        AssetHandle id = m_Registry.Register(AssetType::Model, resolved, shaderHandle);
        m_Registry.Save();

        if (auto it = m_ModelCache.find(id); it != m_ModelCache.end())
            return id;

        try {
            auto model = std::make_shared<Model>(resolved, shader);
            m_ModelCache[id] = model;
            return id;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Model load failed: " << resolved << " (" << e.what() << ")\n";
            m_Registry.Remove(id);
            m_Registry.Save();
            return InvalidAssetHandle;
        }
    }

    AssetHandle AssetManager::LoadTexture2D(const std::string& path) {
        std::string resolved = Content::Resolve(path);
        if (!Content::Exists(resolved)) {
            std::cout << "[AssetManager] Texture file missing: " << resolved << "\n";
            return InvalidAssetHandle;
        }

        AssetHandle id = m_Registry.Register(AssetType::Texture2D, resolved);
        m_Registry.Save();

        if (auto it = m_TextureCache.find(id); it != m_TextureCache.end())
            return id;

        try {
            auto tex = std::make_shared<Texture2D>(resolved);
            m_TextureCache[id] = tex;
            return id;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Texture load failed: " << resolved << " (" << e.what() << ")\n";
            m_Registry.Remove(id);
            m_Registry.Save();
            return InvalidAssetHandle;
        }
    }
    // --- GetShader ---
    std::shared_ptr<Shader> AssetManager::GetShader(AssetHandle shaderHandle) {
        if (shaderHandle == InvalidAssetHandle) return nullptr;

        if (auto it = m_ShaderCache.find(shaderHandle); it != m_ShaderCache.end())
            return it->second;

        const AssetMetadata* meta = m_Registry.Get(shaderHandle);
        if (!meta || meta->Type != AssetType::Shader) return nullptr;

        if (!std::filesystem::exists(meta->Path)) {
            std::cout << "[AssetManager] Shader missing on disk, removing: " << meta->Path << "\n";
            m_Registry.Remove(shaderHandle);
            m_Registry.Save();
            return nullptr;
        }

        try {
            auto shader = std::make_shared<Shader>(meta->Path);
            m_ShaderCache[shaderHandle] = shader;
            return shader;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Shader load failed, removing registry entry: " << meta->Path
                << " (" << e.what() << ")\n";
            m_Registry.Remove(shaderHandle);
            m_Registry.Save();
            return nullptr;
        }
    }

    // --- GetModel ---
    std::shared_ptr<Model> AssetManager::GetModel(AssetHandle modelHandle) {
        if (modelHandle == InvalidAssetHandle) return nullptr;
        if (auto it = m_ModelCache.find(modelHandle); it != m_ModelCache.end())
            return it->second;

        const AssetMetadata* meta = m_Registry.Get(modelHandle);
        if (!meta || meta->Type != AssetType::Model) return nullptr;

        if (!Content::Exists(meta->Path)) {
            std::cout << "[AssetManager] Missing model on disk: " << meta->Path << "\n";
            return nullptr;
        }

        auto shader = GetShader(meta->Shader);
        if (!shader) {
            std::cout << "[AssetManager] Model missing shader: " << meta->Path << "\n";
            return nullptr;
        }

        try {
            auto model = std::make_shared<Model>(meta->Path, shader);
            m_ModelCache[modelHandle] = model;
            return model;
        }
        catch (...) {
            std::cout << "[AssetManager] Model load threw: " << meta->Path << "\n";
            return nullptr;
        }
    }


    // --- GetTexture2D ---
    std::shared_ptr<Texture2D> AssetManager::GetTexture2D(AssetHandle texHandle) {
        if (texHandle == InvalidAssetHandle) return nullptr;

        if (auto it = m_TextureCache.find(texHandle); it != m_TextureCache.end())
            return it->second;

        const AssetMetadata* meta = m_Registry.Get(texHandle);
        if (!meta || meta->Type != AssetType::Texture2D) return nullptr;

        if (!std::filesystem::exists(meta->Path)) {
            std::cout << "[AssetManager] Texture missing on disk, removing: " << meta->Path << "\n";
            m_Registry.Remove(texHandle);
            m_Registry.Save();
            return nullptr;
        }

        try {
            auto tex = std::make_shared<Texture2D>(meta->Path);
            m_TextureCache[texHandle] = tex;
            return tex;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Texture load failed, removing registry entry: " << meta->Path
                << " (" << e.what() << ")\n";
            m_Registry.Remove(texHandle);
            m_Registry.Save();
            return nullptr;
        }
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