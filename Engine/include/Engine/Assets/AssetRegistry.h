#pragma once
#include "Engine/Assets/AssetTypes.h"     // includes AssetHandle.h

#include <string>
#include <unordered_map>
#include <optional>

namespace Engine {

    struct AssetMetadata {
        AssetType Type = AssetType::None;
        std::string Path;         // normalized, forward slashes
        AssetHandle Shader = 0;   // used by Model assets (default shader handle)
    };

    class AssetRegistry {
    public:
        explicit AssetRegistry(const std::string& registryPath = "Assets/Project/asset_registry.json");

        bool Load();
        bool Save() const;

        // Register/import (returns existing handle if path already registered for that type)
        AssetHandle Register(AssetType type, const std::string& path, AssetHandle shader = 0);

        bool Exists(AssetHandle id) const;
        std::optional<AssetHandle> FindByPath(AssetType type, const std::string& path) const;
        const AssetMetadata* Get(AssetHandle id) const;

        bool UpdatePath(AssetHandle id, const std::string& newPath);
        bool Remove(AssetHandle id);

        const std::unordered_map<AssetHandle, AssetMetadata>& GetAll() const { return m_Assets; }
        AssetHandle GetNextID() const { return m_NextID; }

    private:
        static std::string NormalizePath(const std::string& p);

    private:
        std::string m_RegistryPath;
        AssetHandle m_NextID = 1;

        std::unordered_map<AssetHandle, AssetMetadata> m_Assets;
        std::unordered_map<std::string, AssetHandle> m_PathToHandle; // key = type + "|" + path
    };

} // namespace Engine