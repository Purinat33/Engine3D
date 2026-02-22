#include "pch.h"
#include "Engine/Assets/AssetManager.h"

#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/Model.h"

#include <stdexcept>
#include <algorithm>

namespace Engine {

    AssetManager& AssetManager::Get() {
        static AssetManager s_Instance;
        return s_Instance;
    }

    std::string AssetManager::NormalizePath(const std::string& p) {
        std::string out = p;
        std::replace(out.begin(), out.end(), '/', '\\');
        return out;
    }

    std::string AssetManager::MakeModelKey(const std::string& path, AssetHandle shaderHandle) {
        return NormalizePath(path) + "|" + std::to_string(shaderHandle);
    }

    // Shaders
    AssetHandle AssetManager::LoadShader(const std::string& filepath) {
        std::string key = NormalizePath(filepath);

        if (auto it = m_ShaderKeyToHandle.find(key); it != m_ShaderKeyToHandle.end())
            return it->second;

        AssetHandle h = NewHandle();
        auto shader = std::make_shared<Shader>(filepath);

        m_ShaderKeyToHandle[key] = h;
        m_Shaders[h] = shader;
        m_ShaderPath[h] = key;
        return h;
    }

    std::shared_ptr<Shader> AssetManager::GetShader(AssetHandle handle) const {
        if (handle == InvalidAssetHandle) return nullptr;
        auto it = m_Shaders.find(handle);
        if (it == m_Shaders.end())
            throw std::runtime_error("GetShader: invalid handle " + std::to_string(handle));
        return it->second;
    }

    const std::string& AssetManager::GetShaderPath(AssetHandle handle) const {
        auto it = m_ShaderPath.find(handle);
        if (it == m_ShaderPath.end())
            throw std::runtime_error("GetShaderPath: invalid handle " + std::to_string(handle));
        return it->second;
    }

    // Textures
    AssetHandle AssetManager::LoadTexture2D(const std::string& filepath) {
        std::string key = NormalizePath(filepath);

        if (auto it = m_TextureKeyToHandle.find(key); it != m_TextureKeyToHandle.end())
            return it->second;

        AssetHandle h = NewHandle();
        auto tex = std::make_shared<Texture2D>(filepath);

        m_TextureKeyToHandle[key] = h;
        m_Textures[h] = tex;
        return h;
    }

    std::shared_ptr<Texture2D> AssetManager::GetTexture2D(AssetHandle handle) const {
        if (handle == InvalidAssetHandle) return nullptr;
        auto it = m_Textures.find(handle);
        if (it == m_Textures.end())
            throw std::runtime_error("GetTexture2D: invalid handle " + std::to_string(handle));
        return it->second;
    }

    // Models
    AssetHandle AssetManager::LoadModel(const std::string& filepath, AssetHandle defaultShaderHandle) {
        if (defaultShaderHandle == InvalidAssetHandle)
            throw std::runtime_error("LoadModel: defaultShaderHandle invalid");

        std::string key = MakeModelKey(filepath, defaultShaderHandle);

        if (auto it = m_ModelKeyToHandle.find(key); it != m_ModelKeyToHandle.end())
            return it->second;

        auto shader = GetShader(defaultShaderHandle);
        AssetHandle h = NewHandle();
        auto model = std::make_shared<Model>(filepath, shader);

        m_ModelKeyToHandle[key] = h;
        m_Models[h] = model;
        m_ModelInfo[h] = ModelInfo{ NormalizePath(filepath), defaultShaderHandle };
        return h;
    }

    std::shared_ptr<Model> AssetManager::GetModel(AssetHandle handle) const {
        if (handle == InvalidAssetHandle) return nullptr;
        auto it = m_Models.find(handle);
        if (it == m_Models.end())
            throw std::runtime_error("GetModel: invalid handle " + std::to_string(handle));
        return it->second;
    }

    AssetManager::ModelInfo AssetManager::GetModelInfo(AssetHandle handle) const {
        auto it = m_ModelInfo.find(handle);
        if (it == m_ModelInfo.end())
            throw std::runtime_error("GetModelInfo: invalid handle " + std::to_string(handle));
        return it->second;
    }

} // namespace Engine