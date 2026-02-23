#pragma once
#include "Engine/Assets/AssetHandle.h"

#include <string>
#include <cstdint>

namespace Engine {

    enum class AssetType : uint8_t {
        None = 0,
        Shader,
        Model,
        Texture2D
    };

    inline const char* AssetTypeToString(AssetType t) {
        switch (t) {
        case AssetType::Shader:    return "Shader";
        case AssetType::Model:     return "Model";
        case AssetType::Texture2D: return "Texture2D";
        default:                   return "None";
        }
    }

    inline AssetType AssetTypeFromString(const std::string& s) {
        if (s == "Shader") return AssetType::Shader;
        if (s == "Model") return AssetType::Model;
        if (s == "Texture2D") return AssetType::Texture2D;
        return AssetType::None;
    }

} // namespace Engine