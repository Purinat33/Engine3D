#pragma once
#include "Engine/Core/Window.h"

struct GLFWwindow;

namespace Engine {

    class WindowsWindow final : public Window {
    public:
        explicit WindowsWindow(const WindowProps& props);
        ~WindowsWindow() override;

        void OnUpdate() override;
        bool ShouldClose() const override;

        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

        void SetEventCallback(const EventCallbackFn& callback) override { m_EventCallback = callback; }

        void* GetNativeWindow() const override { return m_Window; }

        void SetCursorMode(bool enabled) override;
        bool IsFocused() const override;

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        GLFWwindow* m_Window = nullptr;

        EventCallbackFn m_EventCallback;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        std::string m_Title;
    };

} // namespace Engine