#include "pch.h"
#include "Engine/Assets/AssetRegistry.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cctype>

namespace Engine {

    static std::string ReadAllText(const std::string& path) {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (!in) return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static void EnsureParentDir(const std::string& filePath) {
        std::error_code ec;
        auto parent = std::filesystem::path(filePath).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent, ec);
    }

    static bool ExtractUInt(const std::string& src, const std::string& key, uint32_t& out) {
        // finds: "key": 123
        auto pos = src.find("\"" + key + "\"");
        if (pos == std::string::npos) return false;
        pos = src.find(':', pos);
        if (pos == std::string::npos) return false;
        pos++;

        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) pos++;

        size_t end = pos;
        while (end < src.size() && isdigit((unsigned char)src[end])) end++;

        if (end == pos) return false;
        out = (uint32_t)std::stoul(src.substr(pos, end - pos));
        return true;
    }

    static bool ExtractString(const std::string& src, const std::string& key, std::string& out) {
        // finds: "key": "value"
        auto pos = src.find("\"" + key + "\"");
        if (pos == std::string::npos) return false;
        pos = src.find(':', pos);
        if (pos == std::string::npos) return false;
        pos++;

        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) pos++;
        if (pos >= src.size() || src[pos] != '"') return false;
        pos++;

        auto end = src.find('"', pos);
        if (end == std::string::npos) return false;

        out = src.substr(pos, end - pos);
        return true;
    }

    AssetRegistry::AssetRegistry(const std::string& registryPath)
        : m_RegistryPath(registryPath) {
    }

    std::string AssetRegistry::NormalizePath(const std::string& p) {
        std::filesystem::path fp(p);
        fp = fp.lexically_normal();
        return fp.generic_string(); // forward slashes
    }

    bool AssetRegistry::Load() {
        m_Assets.clear();
        m_PathToHandle.clear();
        m_NextID = 1;

        std::string txt = ReadAllText(m_RegistryPath);
        if (txt.empty())
            return false;

        // nextID
        uint32_t next = 1;
        if (ExtractUInt(txt, "nextID", next))
            m_NextID = (next == 0 ? 1 : next);

        // parse objects inside "assets": [ ... ]
        auto assetsPos = txt.find("\"assets\"");
        if (assetsPos == std::string::npos)
            return true;

        auto arrayStart = txt.find('[', assetsPos);
        auto arrayEnd = txt.find(']', assetsPos);
        if (arrayStart == std::string::npos || arrayEnd == std::string::npos || arrayEnd <= arrayStart)
            return true;

        size_t pos = arrayStart;
        while (true) {
            auto objStart = txt.find('{', pos);
            if (objStart == std::string::npos || objStart > arrayEnd) break;

            auto objEnd = txt.find('}', objStart);
            if (objEnd == std::string::npos || objEnd > arrayEnd) break;

            std::string obj = txt.substr(objStart, (objEnd - objStart) + 1);

            uint32_t id = 0;
            std::string typeStr, pathStr;
            uint32_t shader = 0;

            if (ExtractUInt(obj, "id", id) &&
                ExtractString(obj, "type", typeStr) &&
                ExtractString(obj, "path", pathStr))
            {
                AssetMetadata meta;
                meta.Type = AssetTypeFromString(typeStr);
                meta.Path = NormalizePath(pathStr);

                uint32_t sh = 0;
                if (ExtractUInt(obj, "shader", sh))
                    meta.Shader = sh;

                if (id != 0 && meta.Type != AssetType::None && !meta.Path.empty()) {
                    m_Assets[id] = meta;
                    std::string key = std::string(AssetTypeToString(meta.Type)) + "|" + meta.Path;
                    m_PathToHandle[key] = id;
                }
            }

            pos = objEnd + 1;
        }

        return true;
    }

    bool AssetRegistry::Save() const {
        EnsureParentDir(m_RegistryPath);

        std::ofstream out(m_RegistryPath, std::ios::out | std::ios::trunc);
        if (!out) return false;

        out << "{\n";
        out << "  \"nextID\": " << m_NextID << ",\n";
        out << "  \"assets\": [\n";

        bool first = true;
        for (const auto& [id, meta] : m_Assets) {
            if (!first) out << ",\n";
            first = false;

            out << "    { \"id\": " << id
                << ", \"type\": \"" << AssetTypeToString(meta.Type)
                << "\", \"path\": \"" << meta.Path << "\"";

            if (meta.Type == AssetType::Model && meta.Shader != 0)
                out << ", \"shader\": " << meta.Shader;

            out << " }";
        }

        out << "\n  ]\n";
        out << "}\n";
        return true;
    }

    bool AssetRegistry::Exists(AssetHandle id) const {
        return m_Assets.find(id) != m_Assets.end();
    }

    std::optional<AssetHandle> AssetRegistry::FindByPath(AssetType type, const std::string& path) const {
        std::string key = std::string(AssetTypeToString(type)) + "|" + NormalizePath(path);
        auto it = m_PathToHandle.find(key);
        if (it == m_PathToHandle.end()) return std::nullopt;
        return it->second;
    }

    const AssetMetadata* AssetRegistry::Get(AssetHandle id) const {
        auto it = m_Assets.find(id);
        if (it == m_Assets.end()) return nullptr;
        return &it->second;
    }

    AssetHandle AssetRegistry::Register(AssetType type, const std::string& path, AssetHandle shader) {
        std::string norm = NormalizePath(path);
        std::string key = std::string(AssetTypeToString(type)) + "|" + norm;

        if (auto it = m_PathToHandle.find(key); it != m_PathToHandle.end()) {
            AssetHandle existing = it->second;
            auto& meta = m_Assets[existing];
            if (type == AssetType::Model && shader != 0)
                meta.Shader = shader;
            return existing;
        }

        AssetHandle id = m_NextID++;
        if (id == 0) id = m_NextID++;

        AssetMetadata meta;
        meta.Type = type;
        meta.Path = norm;
        meta.Shader = (type == AssetType::Model) ? shader : 0;

        m_Assets[id] = meta;
        m_PathToHandle[key] = id;
        return id;
    }

    bool AssetRegistry::UpdatePath(AssetHandle id, const std::string& newPath) {
        auto it = m_Assets.find(id);
        if (it == m_Assets.end()) return false;

        std::string oldKey = std::string(AssetTypeToString(it->second.Type)) + "|" + it->second.Path;
        m_PathToHandle.erase(oldKey);

        it->second.Path = NormalizePath(newPath);

        std::string newKey = std::string(AssetTypeToString(it->second.Type)) + "|" + it->second.Path;
        m_PathToHandle[newKey] = id;
        return true;
    }

    bool AssetRegistry::Remove(AssetHandle id) {
        auto it = m_Assets.find(id);
        if (it == m_Assets.end()) return false;

        std::string key = std::string(AssetTypeToString(it->second.Type)) + "|" + it->second.Path;
        m_PathToHandle.erase(key);
        m_Assets.erase(it);
        return true;
    }

} // namespace Engine