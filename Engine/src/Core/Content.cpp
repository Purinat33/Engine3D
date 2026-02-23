#include "pch.h"
#include "Engine/Core/Content.h"
#include <filesystem>

namespace Engine::Content {
    static bool IsAbsoluteOrHasDrive(const std::string& p) {
        if (p.size() > 1 && p[1] == ':') return true;        // C:\...
        if (!p.empty() && (p[0] == '/' || p[0] == '\\')) return true;
        return false;
    }

    bool Exists(const std::string& path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec);
    }

    std::string Resolve(const std::string& path) {
        if (path.empty()) return {};

        // If absolute, just use it
        if (IsAbsoluteOrHasDrive(path))
            return Exists(path) ? path : std::string{};

        // If caller passed "Assets/..." or "EngineContent/...", accept it
        if (Exists(path)) return path;

        // Try Assets/<path>
        {
            std::filesystem::path p = std::filesystem::path(ProjectRoot) / path;
            auto s = p.lexically_normal().generic_string();
            if (Exists(s)) return s;
        }

        // Try EngineContent/<path>
        {
            std::filesystem::path p = std::filesystem::path(EngineRoot) / path;
            auto s = p.lexically_normal().generic_string();
            if (Exists(s)) return s;
        }

        return {};
    }
}