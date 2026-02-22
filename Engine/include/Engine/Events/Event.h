#pragma once
#include <string>
#include <functional>
#include <sstream>

namespace Engine {

    enum class EventType {
        None = 0,

        // Window
        WindowClose, WindowResize,

        // Keyboard
        KeyPressed, KeyReleased, KeyTyped,

        // Mouse
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
        WindowFocus
    };

    enum EventCategory {
        None = 0,
        EventCategoryApplication = 1 << 0,
        EventCategoryInput = 1 << 1,
        EventCategoryKeyboard = 1 << 2,
        EventCategoryMouse = 1 << 3,
        EventCategoryMouseButton = 1 << 4
    };

    class Event {
    public:
        virtual ~Event() = default;

        bool Handled = false;

        virtual EventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool IsInCategory(EventCategory category) const {
            return (GetCategoryFlags() & category) != 0;
        }
    };

    class EventDispatcher {
    public:
        explicit EventDispatcher(Event& e) : m_Event(e) {}

        template<typename T, typename F>
        bool Dispatch(const F& func) {
            if (m_Event.GetEventType() == T::GetStaticType()) {
                m_Event.Handled = func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        Event& m_Event;
    };

#define EVENT_CLASS_TYPE(type) \
    static EventType GetStaticType() { return EventType::type; } \
    EventType GetEventType() const override { return GetStaticType(); } \
    const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(categoryFlags) \
    int GetCategoryFlags() const override { return categoryFlags; }

} // namespace Engine