#pragma once
#include "Engine/Events/Event.h"
#include <sstream>

namespace Engine {

    class WindowFocusEvent : public Event {
    public:
        explicit WindowFocusEvent(bool focused) : m_Focused(focused) {}
        bool IsFocused() const { return m_Focused; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << GetName() << ": " << (m_Focused ? "Focused" : "Unfocused");
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowFocus)
            EVENT_CLASS_CATEGORY(EventCategoryApplication)

    private:
        bool m_Focused = false;
    };

} // namespace Engine