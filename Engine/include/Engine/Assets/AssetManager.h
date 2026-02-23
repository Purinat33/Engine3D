#pragma once
#include "Engine/Assets/AssetRegistry.h"   // brings AssetHandle + AssetType + metadata
#include "Engine/Assets/AssetTypes.h"

#include <memory>
#include <unordered_map>
#include <string>

namespace Engine {

    class Shader;
    class Model;
    class Texture2D;

    class AssetManager {
    public:
        static AssetManager& Get();

        AssetRegistry& Registry() { return m_Registry; }
        const AssetRegistry& Registry() const { return m_Registry; }

        void SaveRegistry() { m_Registry.Save(); }
        void LoadRegistry() { m_Registry.Load(); }

        // Import / Register + load into cache
        AssetHandle LoadShader(const std::string& path);
        AssetHandle LoadModel(const std::string& path, AssetHandle shaderHandle);
        AssetHandle LoadTexture2D(const std::string& path);

        // Resolve handles -> live objects
        std::shared_ptr<Shader> GetShader(AssetHandle shaderHandle);
        std::shared_ptr<Model> GetModel(AssetHandle modelHandle);
        std::shared_ptr<Texture2D> GetTexture2D(AssetHandle texHandle);

        struct ModelInfo {
            std::string Path;
            AssetHandle ShaderHandle = InvalidAssetHandle;
        };

        ModelInfo GetModelInfo(AssetHandle modelHandle) const;
        std::string GetShaderPath(AssetHandle shaderHandle) const;

    private:
        AssetManager();

    private:
        AssetRegistry m_Registry;

        std::unordered_map<AssetHandle, std::shared_ptr<Shader>> m_ShaderCache;
        std::unordered_map<AssetHandle, std::shared_ptr<Model>> m_ModelCache;
        std::unordered_map<AssetHandle, std::shared_ptr<Texture2D>> m_TextureCache;
    };

} // namespace Engine