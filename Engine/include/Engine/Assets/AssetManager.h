#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Assets/AssetHandle.h"

namespace Engine {

    class Shader;
    class Texture2D;
    class Model;

    class AssetManager {
    public:
        static AssetManager& Get();

        // Shaders
        AssetHandle LoadShader(const std::string& filepath);
        std::shared_ptr<Shader> GetShader(AssetHandle handle) const;
        const std::string& GetShaderPath(AssetHandle handle) const;

        // Textures
        AssetHandle LoadTexture2D(const std::string& filepath);
        std::shared_ptr<Texture2D> GetTexture2D(AssetHandle handle) const;

        // Models
        struct ModelInfo {
            std::string Path;
            AssetHandle Shader = InvalidAssetHandle;
        };

        AssetHandle LoadModel(const std::string& filepath, AssetHandle defaultShaderHandle);
        std::shared_ptr<Model> GetModel(AssetHandle handle) const;
        ModelInfo GetModelInfo(AssetHandle handle) const;

    private:
        AssetManager() = default;

        AssetHandle NewHandle() { return ++m_NextHandle; }
        static std::string NormalizePath(const std::string& p);
        static std::string MakeModelKey(const std::string& path, AssetHandle shaderHandle);

    private:
        AssetHandle m_NextHandle = InvalidAssetHandle;

        std::unordered_map<std::string, AssetHandle> m_ShaderKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Shader>> m_Shaders;
        std::unordered_map<AssetHandle, std::string> m_ShaderPath;

        std::unordered_map<std::string, AssetHandle> m_TextureKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Texture2D>> m_Textures;

        std::unordered_map<std::string, AssetHandle> m_ModelKeyToHandle;
        std::unordered_map<AssetHandle, std::shared_ptr<Model>> m_Models;
        std::unordered_map<AssetHandle, ModelInfo> m_ModelInfo;
    };

} // namespace Engine