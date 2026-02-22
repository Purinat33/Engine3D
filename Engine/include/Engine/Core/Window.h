#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <functional>

#include "Engine/Events/Event.h"

namespace Engine {

    struct WindowProps {
        std::string Title = "Engine3D";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class Window {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual void OnUpdate() = 0;
        virtual bool ShouldClose() const = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void* GetNativeWindow() const = 0;

        static std::unique_ptr<Window> Create(const WindowProps& props = WindowProps{});
    };

} // namespace Engine