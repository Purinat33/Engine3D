#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cmath>

#include <glm/glm.hpp>

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Scene/Components.h>

namespace EditorUndo {

    struct TransformSnapshot {
        glm::vec3 Translation{ 0.0f };
        glm::vec3 Rotation{ 0.0f };  // radians
        glm::vec3 Scale{ 1.0f };
    };

    inline TransformSnapshot CaptureTransform(const Engine::Entity& e) {
        TransformSnapshot s;
        if (!e) return s;
        const auto& tc = e.GetComponent<Engine::TransformComponent>();
        s.Translation = tc.Translation;
        s.Rotation = tc.Rotation;
        s.Scale = tc.Scale;
        return s;
    }

    inline void ApplyTransform(Engine::Entity e, const TransformSnapshot& s) {
        if (!e) return;
        auto& tc = e.GetComponent<Engine::TransformComponent>();
        tc.Translation = s.Translation;
        tc.Rotation = s.Rotation;
        tc.Scale = s.Scale;
    }

    inline bool NearlyEqual(float a, float b, float eps = 1e-5f) {
        return std::fabs(a - b) <= eps;
    }

    inline bool NearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-5f) {
        return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps) && NearlyEqual(a.z, b.z, eps);
    }

    inline bool TransformEqual(const TransformSnapshot& a, const TransformSnapshot& b, float eps = 1e-5f) {
        return NearlyEqualVec3(a.Translation, b.Translation, eps) &&
            NearlyEqualVec3(a.Rotation, b.Rotation, eps) &&
            NearlyEqualVec3(a.Scale, b.Scale, eps);
    }

    // --- Entity snapshot for Create/Delete undo ---
    struct EntitySnapshot {
        Engine::UUID ID{};
        std::string Tag{ "Entity" };
        TransformSnapshot Transform{};

        bool HasMeshRenderer = false;
        Engine::MeshRendererComponent MeshRenderer{};

        bool HasDirectionalLight = false;
        Engine::DirectionalLightComponent DirectionalLight{};
    };

    inline EntitySnapshot CaptureEntity(Engine::Entity e) {
        EntitySnapshot s;
        if (!e) return s;

        s.ID = e.GetComponent<Engine::IDComponent>().ID;
        s.Tag = e.GetComponent<Engine::TagComponent>().Tag;
        s.Transform = CaptureTransform(e);

        if (e.HasComponent<Engine::MeshRendererComponent>()) {
            s.HasMeshRenderer = true;
            s.MeshRenderer = e.GetComponent<Engine::MeshRendererComponent>();
        }

        if (e.HasComponent<Engine::DirectionalLightComponent>()) {
            s.HasDirectionalLight = true;
            s.DirectionalLight = e.GetComponent<Engine::DirectionalLightComponent>();
        }

        return s;
    }

    inline Engine::Entity RestoreEntity(Engine::Scene& scene, const EntitySnapshot& s) {
        // If already exists, return it (prevents duplicates on redo)
        Engine::Entity existing = scene.FindEntityByUUID(s.ID);
        if (existing) return existing;

        Engine::Entity e = scene.CreateEntityWithUUID(s.ID, s.Tag.c_str());

        // base comps exist already
        e.GetComponent<Engine::TagComponent>().Tag = s.Tag;
        ApplyTransform(e, s.Transform);

        if (s.HasMeshRenderer && !e.HasComponent<Engine::MeshRendererComponent>())
            e.AddComponent<Engine::MeshRendererComponent>(s.MeshRenderer);

        if (s.HasDirectionalLight && !e.HasComponent<Engine::DirectionalLightComponent>())
            e.AddComponent<Engine::DirectionalLightComponent>(s.DirectionalLight);

        return e;
    }

    inline void DestroyByUUID(Engine::Scene& scene, Engine::UUID id) {
        Engine::Entity e = scene.FindEntityByUUID(id);
        if (e) scene.DestroyEntity(e);
    }

    // ---------------- Command interfaces ----------------

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Undo(Engine::Scene& scene) = 0;
        virtual void Redo(Engine::Scene& scene) = 0;
        virtual const char* Name() const = 0;
    };

    class CommandStack {
    public:
        void Clear() {
            m_Commands.clear();
            m_Index = 0;
        }

        // Execute: performs Redo() now, then pushes it
        void Execute(Engine::Scene& scene, std::unique_ptr<ICommand> cmd) {
            // drop redo branch
            if (m_Index < m_Commands.size())
                m_Commands.erase(m_Commands.begin() + (ptrdiff_t)m_Index, m_Commands.end());

            cmd->Redo(scene);
            m_Commands.emplace_back(std::move(cmd));
            m_Index = m_Commands.size();
        }

        // Commit: push a command whose effect already happened (e.g., gizmo drag)
        void Commit(std::unique_ptr<ICommand> cmd) {
            if (m_Index < m_Commands.size())
                m_Commands.erase(m_Commands.begin() + (ptrdiff_t)m_Index, m_Commands.end());

            m_Commands.emplace_back(std::move(cmd));
            m_Index = m_Commands.size();
        }

        bool CanUndo() const { return m_Index > 0; }
        bool CanRedo() const { return m_Index < m_Commands.size(); }

        void Undo(Engine::Scene& scene) {
            if (!CanUndo()) return;
            m_Index--;
            m_Commands[m_Index]->Undo(scene);
        }

        void Redo(Engine::Scene& scene) {
            if (!CanRedo()) return;
            m_Commands[m_Index]->Redo(scene);
            m_Index++;
        }

    private:
        std::vector<std::unique_ptr<ICommand>> m_Commands;
        size_t m_Index = 0; // points to next redo position
    };

    // ---------------- Concrete commands ----------------

    class TransformCommand final : public ICommand {
    public:
        TransformCommand(Engine::UUID id, const TransformSnapshot& before, const TransformSnapshot& after)
            : m_ID(id), m_Before(before), m_After(after) {
        }

        void Undo(Engine::Scene& scene) override {
            auto e = scene.FindEntityByUUID(m_ID);
            ApplyTransform(e, m_Before);
        }

        void Redo(Engine::Scene& scene) override {
            auto e = scene.FindEntityByUUID(m_ID);
            ApplyTransform(e, m_After);
        }

        const char* Name() const override { return "Transform"; }

    private:
        Engine::UUID m_ID{};
        TransformSnapshot m_Before{}, m_After{};
    };

    class DeleteEntityCommand final : public ICommand {
    public:
        explicit DeleteEntityCommand(const EntitySnapshot& snap)
            : m_Snap(snap) {
        }

        void Undo(Engine::Scene& scene) override {
            RestoreEntity(scene, m_Snap);
        }

        void Redo(Engine::Scene& scene) override {
            DestroyByUUID(scene, m_Snap.ID);
        }

        const char* Name() const override { return "Delete Entity"; }

        Engine::UUID GetID() const { return m_Snap.ID; }

    private:
        EntitySnapshot m_Snap;
    };

    class CreateEntityCommand final : public ICommand {
    public:
        explicit CreateEntityCommand(const EntitySnapshot& snap)
            : m_Snap(snap) {
        }

        void Undo(Engine::Scene& scene) override {
            DestroyByUUID(scene, m_Snap.ID);
        }

        void Redo(Engine::Scene& scene) override {
            RestoreEntity(scene, m_Snap);
        }

        const char* Name() const override { return "Create Entity"; }

        Engine::UUID GetID() const { return m_Snap.ID; }

    private:
        EntitySnapshot m_Snap;
    };

    inline EntitySnapshot MakeDuplicateSnapshot(Engine::Scene& scene, Engine::Entity src, Engine::UUID newID) {
        EntitySnapshot s = CaptureEntity(src);
        s.ID = newID;

        // tag naming
        if (!s.Tag.empty())
            s.Tag = s.Tag + " Copy";
        else
            s.Tag = "Entity Copy";

        return s;
    }

} // namespace EditorUndo