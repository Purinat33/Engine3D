#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace EditorProject {

    static inline std::string ProjectFilePath()
    {
        return "Assets/Project/project.json";
    }

    static inline std::string ReadAllText(const std::string& path)
    {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in) return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static inline bool WriteAllText(const std::string& path, const std::string& text)
    {
        std::filesystem::path p(path);
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(path, std::ios::out | std::ios::binary);
        if (!out) return false;
        out << text;
        return true;
    }

    // Tiny JSON reader for {"startupScene":"..."} (good enough for our file)
    static inline std::string LoadStartupScene()
    {
        const std::string file = ProjectFilePath();
        std::string s = ReadAllText(file);
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

    static inline bool SaveStartupScene(const std::string& scenePath)
    {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"startupScene\": \"" << scenePath << "\"\n";
        ss << "}\n";
        return WriteAllText(ProjectFilePath(), ss.str());
    }

} // namespace EditorProject