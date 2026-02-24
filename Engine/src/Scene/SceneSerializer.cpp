#include "pch.h"
#include "Engine/Scene/SceneSerializer.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"

#include "Engine/Assets/AssetManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace Engine {

    using json = nlohmann::json;

    static json Vec3ToJson(const glm::vec3& v) {
        return json::array({ v.x, v.y, v.z });
    }
    static json Vec4ToJson(const glm::vec4& v) {
        return json::array({ v.x, v.y, v.z, v.w });
    }
    static glm::vec3 JsonToVec3(const json& a) {
        return { a[0].get<float>(), a[1].get<float>(), a[2].get<float>() };
    }

    SceneSerializer::SceneSerializer(Scene& scene)
        : m_Scene(scene) {
    }

    bool SceneSerializer::Serialize(const std::string& filepath) {
        try {
            std::filesystem::path p(filepath);
            if (p.has_parent_path())
                std::filesystem::create_directories(p.parent_path());

            json root;
            root["Scene"] = "Untitled";
            root["Entities"] = json::array();

            auto& reg = m_Scene.Registry();
            auto view = reg.view<IDComponent>();

            auto& assets = AssetManager::Get();

            view.each([&](auto entity, IDComponent& idc) {
                json e;
                e["ID"] = idc.ID;

                // Tag (optional)
                if (reg.any_of<TagComponent>(entity)) {
                    e["Tag"] = reg.get<TagComponent>(entity).Tag;
                }

                // Transform (optional)
                if (reg.any_of<TransformComponent>(entity)) {
                    const auto& tc = reg.get<TransformComponent>(entity);
                    e["Transform"]["Translation"] = Vec3ToJson(tc.Translation);
                    e["Transform"]["Rotation"] = Vec3ToJson(tc.Rotation);
                    e["Transform"]["Scale"] = Vec3ToJson(tc.Scale);
                }

                // MeshRenderer (optional)
                if (reg.any_of<MeshRendererComponent>(entity)) {
                    const auto& mrc = reg.get<MeshRendererComponent>(entity);
                    if (mrc.Model != InvalidAssetHandle) {
                        auto info = assets.GetModelInfo(mrc.Model);
                        e["MeshRenderer"]["ModelPath"] = info.Path;
                        e["MeshRenderer"]["ShaderPath"] = assets.GetShaderPath(info.ShaderHandle);
                    }
                }

                // DirectionalLight (optional)
                if (reg.any_of<DirectionalLightComponent>(entity)) {
                    const auto& lc = reg.get<DirectionalLightComponent>(entity);
                    e["DirectionalLight"]["Color"] = Vec3ToJson(lc.Color);
                    // Direction comes from Transform rotation; don't serialize lc.Direction
                }

                // Spawn
                if (reg.any_of<SpawnPointComponent>(entity)) {
                    e["SpawnPoint"] = true;
                }

                if (reg.any_of<SceneWarpComponent>(entity)) {
                    const auto& sw = reg.get<SceneWarpComponent>(entity);
                    e["SceneWarp"]["TargetScene"] = sw.TargetScene;
                    e["SceneWarp"]["TargetSpawnTag"] = sw.TargetSpawnTag;
                }

                root["Entities"].push_back(e);
                });

            std::ofstream out(filepath, std::ios::out | std::ios::trunc);
            if (!out) {
                std::cerr << "[SceneSerializer] Failed to open for write: " << filepath << "\n";
                return false;
            }

            out << root.dump(2);
            return true;
        }
        catch (const std::exception& ex) {
            std::cerr << "[SceneSerializer] Serialize exception: " << ex.what() << "\n";
            return false;
        }
    }

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        try {
            std::ifstream in(filepath, std::ios::in);
            if (!in) {
                std::cerr << "[SceneSerializer] Failed to open for read: " << filepath << "\n";
                return false;
            }

            json root;
            in >> root;

            if (!root.contains("Entities") || !root["Entities"].is_array()) {
                std::cerr << "[SceneSerializer] Invalid scene file (no Entities array)\n";
                return false;
            }

            // Clear scene registry
            m_Scene.Clear();

            auto& assets = AssetManager::Get();

            for (const auto& e : root["Entities"]) {
                UUID id = e.value("ID", (UUID)0);
                std::string tag = e.value("Tag", "Entity");

                Entity ent = m_Scene.CreateEntityWithUUID(id, tag.c_str());

                // Transform
                if (e.contains("Transform")) {
                    auto& tc = ent.GetComponent<TransformComponent>();
                    const auto& t = e["Transform"];
                    if (t.contains("Translation")) tc.Translation = JsonToVec3(t["Translation"]);
                    if (t.contains("Rotation"))    tc.Rotation = JsonToVec3(t["Rotation"]);
                    if (t.contains("Scale"))       tc.Scale = JsonToVec3(t["Scale"]);
                }

                // MeshRenderer
                if (e.contains("MeshRenderer")) {
                    const auto& mr = e["MeshRenderer"];
                    std::string modelPath = mr.value("ModelPath", "");
                    std::string shaderPath = mr.value("ShaderPath", "");

                    if (!modelPath.empty() && !shaderPath.empty()) {
                        AssetHandle sh = assets.LoadShader(shaderPath);
                        AssetHandle mo = assets.LoadModel(modelPath, sh);
                        ent.AddComponent<MeshRendererComponent>(mo);
                    }
                }

                // DirectionalLight
                if (e.contains("DirectionalLight")) {
                    const auto& dl = e["DirectionalLight"];
                    glm::vec3 col = dl.contains("Color") ? JsonToVec3(dl["Color"]) : glm::vec3(1.0f);

                    // Direction will be derived from Transform rotation at runtime,
                    // but keep a reasonable default for the component field:
                    glm::vec3 dir = glm::vec3(0.4f, 0.8f, -0.3f);

                    ent.AddComponent<DirectionalLightComponent>(dir, col);
                }

                // Spawn
                if (e.contains("SpawnPoint") && e["SpawnPoint"].get<bool>()) {
                    ent.AddComponent<SpawnPointComponent>();
                }

                if (e.contains("SceneWarp")) {
                    auto& sw = ent.AddComponent<SceneWarpComponent>();
                    const auto& j = e["SceneWarp"];
                    sw.TargetScene = j.value("TargetScene", "Assets/Scenes/Sandbox.scene");
                    sw.TargetSpawnTag = j.value("TargetSpawnTag", "");
                }
            }

            return true;
        }
        catch (const std::exception& ex) {
            std::cerr << "[SceneSerializer] Deserialize exception: " << ex.what() << "\n";
            return false;
        }
    }

} // namespace Engine