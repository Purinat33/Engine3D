#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include "CommandStack.h"

class EditorSceneManager {
public:
    EditorSceneManager(Engine::Scene& scene,
        EditorUndo::CommandStack& cmdStack,
        std::function<void()> onSceneChanged)
        : m_Scene(scene), m_Serializer(scene), m_CmdStack(cmdStack), m_OnSceneChanged(std::move(onSceneChanged))
    {
        RefreshSceneList();
    }

    const std::string& GetCurrentPath() const { return m_CurrentPath; }
    bool IsDirty() const { return m_Dirty; }
    void MarkDirty() { m_Dirty = true; }
    void MarkClean() { m_Dirty = false; }

    std::string GetDisplayName() const {
        if (m_CurrentPath.empty()) return "Untitled";
        return std::filesystem::path(m_CurrentPath).filename().string();
    }

    const std::vector<std::string>& GetSceneList() const { return m_Scenes; }
    void RefreshSceneList(const std::string& root = "Assets/Scenes") {
        m_Scenes.clear();
        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
            return;

        for (auto& it : std::filesystem::recursive_directory_iterator(root, ec)) {
            if (ec) break;
            if (!it.is_regular_file()) continue;
            auto p = it.path();
            if (p.extension() == ".scene") {
                // store as generic string with forward slashes (portable)
                m_Scenes.push_back(p.generic_string());
            }
        }
        std::sort(m_Scenes.begin(), m_Scenes.end());
    }

    // Create an empty scene with a default light (optional but nice)
    void NewScene() {
        m_Scene.Clear();
        auto sun = m_Scene.CreateEntity("SunLight");
        auto& dl = sun.AddComponent<Engine::DirectionalLightComponent>();
        dl.Direction = { 0.4f, 0.8f, -0.3f };
        dl.Color = { 1.0f, 1.0f, 1.0f };

        m_CurrentPath.clear();
        m_CmdStack.Clear();
        m_Dirty = false;
        if (m_OnSceneChanged) m_OnSceneChanged();
    }

    bool OpenScene(const std::string& path) {
        if (path.empty()) return false;

        // Clear then load
        m_Scene.Clear();
        bool ok = m_Serializer.Deserialize(path);
        if (!ok) {
            // If load fails, keep scene empty
            m_CurrentPath.clear();
            m_CmdStack.Clear();
            m_Dirty = false;
            if (m_OnSceneChanged) m_OnSceneChanged();
            return false;
        }

        m_CurrentPath = path;
        m_CmdStack.Clear();
        m_Dirty = false;
        RefreshSceneList();
        if (m_OnSceneChanged) m_OnSceneChanged();
        return true;
    }

    bool Save() {
        if (m_CurrentPath.empty())
            return false;
        m_Serializer.Serialize(m_CurrentPath);
        m_Dirty = false;
        RefreshSceneList();
        return true;
    }

    bool SaveAs(const std::string& path) {
        if (path.empty()) return false;
        m_CurrentPath = path;
        m_Serializer.Serialize(m_CurrentPath);
        m_Dirty = false;
        RefreshSceneList();
        return true;
    }

private:
    Engine::Scene& m_Scene;
    Engine::SceneSerializer m_Serializer;

    EditorUndo::CommandStack& m_CmdStack;
    std::function<void()> m_OnSceneChanged;

    std::string m_CurrentPath;
    bool m_Dirty = false;
    std::vector<std::string> m_Scenes;
};