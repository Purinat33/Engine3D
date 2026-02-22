#pragma once
#include <memory>
#include "Engine/Core/Window.h"

namespace Engine {

    class WindowCloseEvent;
    class WindowResizeEvent;
    class Event;

    class Application {
    public:
        Application();
        void Run();

        void OnEvent(Event& e);

        bool m_CaptureMouse = true;
        bool m_HasFocus = true;

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

    private:
        std::unique_ptr<Window> m_Window;
        bool m_Running = true;
    };

} // namespace Engine