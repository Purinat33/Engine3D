#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Engine::ProjectSettings {

    inline std::string ProjectFilePath() {
        return "Assets/Project/project.json";
    }

    inline std::string ReadAllText(const std::string& path) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in) return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Very small JSON parse for {"startupScene":"..."} (good enough for our file)
    inline std::string LoadStartupScenePath() {
        const std::string s = ReadAllText(ProjectFilePath());
        if (s.empty()) return {};

        auto keyPos = s.find("\"startupScene\"");
        if (keyPos == std::string::npos) return {};
        auto colon = s.find(':', keyPos);
        if (colon == std::string::npos) return {};

        auto q1 = s.find('"', colon + 1);
        if (q1 == std::string::npos) return {};
        auto q2 = s.find('"', q1 + 1);
        if (q2 == std::string::npos) return {};

        return s.substr(q1 + 1, q2 - (q1 + 1));
    }

    inline std::string GetStartupSceneOrDefault(const std::string& fallback = "Assets/Scenes/Sandbox.scene") {
        std::string p = LoadStartupScenePath();
        if (p.empty())
            return fallback;

        // If file doesn’t exist, fall back
        std::error_code ec;
        if (!std::filesystem::exists(p, ec))
            return fallback;

        return p;
    }

} // namespace Engine::ProjectSettings