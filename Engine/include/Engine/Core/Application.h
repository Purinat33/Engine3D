#pragma once
#include <memory>
#include "Engine/Core/Window.h"

namespace Engine {

    class Application {
    public:
        Application();
        void Run();

    private:
        std::unique_ptr<Window> m_Window;
    };

} // namespace Engine