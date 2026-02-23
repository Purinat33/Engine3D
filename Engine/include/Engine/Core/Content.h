#pragma once
#include <string>
#include <filesystem>

namespace Engine::Content {

    inline constexpr const char* ProjectRoot = "Assets";
    inline constexpr const char* EngineRoot = "EngineContent";

    inline std::string Normalize(const std::string& p) {
        std::filesystem::path fp(p);
        return fp.lexically_normal().generic_string();
    }

    inline bool Exists(const std::string& p) {
        std::error_code ec;
        return std::filesystem::exists(std::filesystem::path(p), ec);
    }

    // Resolution rule (simple):
    // 1) if path exists as-is -> use it
    // 2) if relative -> try Assets/<path>
    // 3) if relative -> try EngineContent/<path>
    inline std::string Resolve(const std::string& path) {
        if (path.empty()) return path;

        if (Exists(path))
            return Normalize(path);

        std::filesystem::path fp(path);
        if (fp.is_relative()) {
            std::string p1 = Normalize(std::string(ProjectRoot) + "/" + path);
            if (Exists(p1)) return p1;

            std::string p2 = Normalize(std::string(EngineRoot) + "/" + path);
            if (Exists(p2)) return p2;
        }

        return Normalize(path);
    }

} // namespace Engine::Content