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

    private:
        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

    private:
        std::unique_ptr<Window> m_Window;
        bool m_Running = true;
    };

} // namespace Engine