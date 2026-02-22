#pragma once
#include <string>
#include <cstdint>
#include <memory>

namespace Engine {

    struct WindowProps {
        std::string Title = "Engine3D";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class Window {
    public:
        virtual ~Window() = default;

        virtual void OnUpdate() = 0;
        virtual bool ShouldClose() const = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        // For advanced usage later (ImGui, raw GLFW calls, etc.)
        virtual void* GetNativeWindow() const = 0;

        static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps{});
    };

} // namespace Engine