#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Assets/AssetHandle.h"

namespace Engine {

    class Shader;
    class Texture2D;
    class Model;

    // Simple cache-based asset manager.
    // Keys are file paths (+ shader handle for models).
    class AssetManager {
    public:
        static AssetManager& Get();

        // Shaders
        AssetHandle LoadShader(const std::string& filepath);
        std::shared_ptr<Shader> GetShader(AssetHandle handle) const;

        // Textures
        AssetHandle LoadTexture2D(const std::string& filepath);
        std::shared_ptr<Texture2D> GetTexture2D(AssetHandle handle) const;

        // Models (need a shader handle because your Model constructor needs a Shader)
        AssetHandle LoadModel(const std::string& filepath, AssetHandle defaultShaderHandle);
        std::shared_ptr<Model> GetModel(AssetHandle handle) const;

    private:
        AssetManager() = default;

        AssetHandle NewHandle() { return ++m_NextHandle; }
        static std::string NormalizePath(const std::string& p);
        static std::string MakeModelKey(const std::string& path, AssetHandle shaderHandle);

    private:
        AssetHandle m_NextHandle = InvalidAssetHandle;

        // Shader cache
        std::unordered_map<std::string, AssetHandle> m_ShaderKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Shader>> m_Shaders;

        // Texture cache
        std::unordered_map<std::string, AssetHandle> m_TextureKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Texture2D>> m_Textures;

        // Model cache (key: normalizedPath|shaderHandle)
        std::unordered_map<std::string, AssetHandle> m_ModelKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Model>> m_Models;
    };

} // namespace Engine