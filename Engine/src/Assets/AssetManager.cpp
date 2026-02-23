#include "pch.h"
#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Model.h"
#include "Engine/Renderer/Texture2D.h"

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
        namespace fs = std::filesystem;
        std::string p = path;

        if (!fs::exists(p)) {
            std::cout << "[AssetManager] Shader file missing: " << p << "\n";
            return InvalidAssetHandle;
        }

        // If already registered, reuse handle
        if (auto existing = m_Registry.FindByPath(AssetType::Shader, p)) {
            AssetHandle id = *existing;
            if (m_ShaderCache.find(id) != m_ShaderCache.end())
                return id;

            try {
                auto shader = std::make_shared<Shader>(m_Registry.Get(id)->Path);
                m_ShaderCache[id] = shader;
                return id;
            }
            catch (const std::exception& e) {
                std::cout << "[AssetManager] Shader load failed, removing registry entry: " << e.what() << "\n";
                m_Registry.Remove(id);
                m_Registry.Save();
                return InvalidAssetHandle;
            }
        }

        // Try load FIRST
        std::shared_ptr<Shader> shader;
        try {
            shader = std::make_shared<Shader>(p);
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Shader load failed: " << p << " (" << e.what() << ")\n";
            return InvalidAssetHandle;
        }

        // Register only after success
        AssetHandle id = m_Registry.Register(AssetType::Shader, p);
        m_Registry.Save();

        m_ShaderCache[id] = shader;
        return id;
    }

    AssetHandle AssetManager::LoadModel(const std::string& path, AssetHandle shaderHandle) {
        namespace fs = std::filesystem;
        std::string p = path;

        if (!fs::exists(p)) {
            std::cout << "[AssetManager] Model file missing: " << p << "\n";
            return InvalidAssetHandle;
        }

        // Shader must exist
        auto shader = GetShader(shaderHandle);
        if (!shader) {
            std::cout << "[AssetManager] Missing/invalid shader handle for model: " << p << "\n";
            return InvalidAssetHandle;
        }

        // If already registered, reuse handle (but also validate it can load)
        if (auto existing = m_Registry.FindByPath(AssetType::Model, p)) {
            AssetHandle id = *existing;

            // Update shader association in registry (if needed)
            m_Registry.Register(AssetType::Model, p, shaderHandle);
            m_Registry.Save();

            if (m_ModelCache.find(id) != m_ModelCache.end())
                return id;

            try {
                const AssetMetadata* meta = m_Registry.Get(id);
                auto model = std::make_shared<Model>(meta->Path, shader);
                m_ModelCache[id] = model;
                return id;
            }
            catch (const std::exception& e) {
                std::cout << "[AssetManager] Model load failed, removing registry entry: " << p
                    << " (" << e.what() << ")\n";
                m_Registry.Remove(id);
                m_Registry.Save();
                return InvalidAssetHandle;
            }
        }

        // Try load FIRST
        std::shared_ptr<Model> model;
        try {
            model = std::make_shared<Model>(p, shader);
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Model load failed: " << p << " (" << e.what() << ")\n";
            return InvalidAssetHandle;
        }

        // Register only after success
        AssetHandle id = m_Registry.Register(AssetType::Model, p, shaderHandle);
        m_Registry.Save();

        m_ModelCache[id] = model;
        return id;
    }

    AssetHandle AssetManager::LoadTexture2D(const std::string& path) {
        namespace fs = std::filesystem;
        std::string p = path;

        if (!fs::exists(p)) {
            std::cout << "[AssetManager] Texture file missing: " << p << "\n";
            return InvalidAssetHandle;
        }

        if (auto existing = m_Registry.FindByPath(AssetType::Texture2D, p)) {
            AssetHandle id = *existing;
            if (m_TextureCache.find(id) != m_TextureCache.end())
                return id;

            try {
                const AssetMetadata* meta = m_Registry.Get(id);
                auto tex = std::make_shared<Texture2D>(meta->Path);
                m_TextureCache[id] = tex;
                return id;
            }
            catch (const std::exception& e) {
                std::cout << "[AssetManager] Texture load failed, removing registry entry: " << p
                    << " (" << e.what() << ")\n";
                m_Registry.Remove(id);
                m_Registry.Save();
                return InvalidAssetHandle;
            }
        }

        std::shared_ptr<Texture2D> tex;
        try {
            tex = std::make_shared<Texture2D>(p);
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Texture load failed: " << p << " (" << e.what() << ")\n";
            return InvalidAssetHandle;
        }

        AssetHandle id = m_Registry.Register(AssetType::Texture2D, p);
        m_Registry.Save();

        m_TextureCache[id] = tex;
        return id;
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

        if (!std::filesystem::exists(meta->Path)) {
            std::cout << "[AssetManager] Model missing on disk, removing: " << meta->Path << "\n";
            m_Registry.Remove(modelHandle);
            m_Registry.Save();
            return nullptr;
        }

        auto shader = GetShader(meta->Shader);
        if (!shader) {
            std::cout << "[AssetManager] Model has missing shader handle, keeping entry but not loading: "
                << meta->Path << "\n";
            return nullptr;
        }

        try {
            auto model = std::make_shared<Model>(meta->Path, shader);
            m_ModelCache[modelHandle] = model;
            return model;
        }
        catch (const std::exception& e) {
            std::cout << "[AssetManager] Model load failed, removing registry entry: " << meta->Path
                << " (" << e.what() << ")\n";
            m_Registry.Remove(modelHandle);
            m_Registry.Save();
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