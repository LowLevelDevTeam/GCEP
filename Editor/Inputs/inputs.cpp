#include "inputs.hpp"

// STL
#include <algorithm>

namespace gcep
{
    InputSystem* InputSystem::s_scrollTarget = nullptr;

    // Lifecycle

    InputSystem::InputSystem()
        : m_window(Window::getInstance().getGlfwWindow())
    {
        glfwGetCursorPos(m_window, &m_prevPosX, &m_prevPosY);
        m_mouse.posX = m_prevPosX;
        m_mouse.posY = m_prevPosY;

        s_scrollTarget = this;
        glfwSetScrollCallback(m_window, scrollCallback);
    }

    InputSystem::~InputSystem()
    {
        if (s_scrollTarget == this)
        {
            glfwSetScrollCallback(m_window, nullptr);
            s_scrollTarget = nullptr;
        }
    }

    // Frame pump

    void InputSystem::update()
    {
        for (auto& [name, binding] : m_bindings)
        {
            if (binding.device == InputDevice::Keyboard)
            {
                if (!m_currKeys.count(binding.key))
                {
                    m_prevKeys[binding.key] = false;
                    m_currKeys[binding.key] = glfwKeyDown(binding.key);
                }
            }
            else
            {
                if (!m_currMouse.count(binding.key))
                {
                    m_prevMouse[binding.key] = false;
                    m_currMouse[binding.key] = glfwMouseDown(binding.key);
                }
            }
        }

        m_prevKeys = m_currKeys;
        for (auto& [key, _] : m_prevKeys)
            m_currKeys[key] = glfwKeyDown(key);

        m_prevMouse = m_currMouse;
        for (auto& [btn, _] : m_prevMouse)
            m_currMouse[btn] = glfwMouseDown(btn);

        // Mouse position + delta
        double cx, cy;
        glfwGetCursorPos(m_window, &cx, &cy);
        m_mouse.deltaX = cx - m_prevPosX;
        m_mouse.deltaY = cy - m_prevPosY;
        m_prevPosX     = cx;
        m_prevPosY     = cy;
        m_mouse.posX   = cx;
        m_mouse.posY   = cy;

        // Scroll: consume pending accumulator
        m_mouse.scrollX  = m_pendingScrollX;
        m_mouse.scrollY  = m_pendingScrollY;
        m_pendingScrollX = 0.0;
        m_pendingScrollY = 0.0;

        // Callbacks
        for (auto& [name, binding] : m_bindings)
        {
            KeyState s = (binding.device == InputDevice::Mouse)
                        ? computeState(m_prevMouse.count(binding.key) && m_prevMouse.at(binding.key), glfwMouseDown(binding.key))
                        : keyState(binding.key);

            bool fire = (binding.trigger == TriggerOn::Pressed  && s == KeyState::Pressed)
                     || (binding.trigger == TriggerOn::Held     && s == KeyState::Held)
                     || (binding.trigger == TriggerOn::Released && s == KeyState::Released);

            if (fire && binding.callback)
                binding.callback();
        }
    }

    KeyState InputSystem::computeState(bool prev, bool curr) const
    {
        if (!prev && !curr)   return KeyState::Up;
        if (!prev &&  curr)   return KeyState::Pressed;
        if ( prev &&  curr)   return KeyState::Held;
        /*   prev && !curr */ return KeyState::Released;
    }

    bool InputSystem::glfwKeyDown(int key) const
    {
        return glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    bool InputSystem::glfwMouseDown(int button) const
    {
        return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
    }

    // Keys — lazy registration: first query for a key seeds it into the maps
    KeyState InputSystem::keyState(int key) const
    {
        bool curr = glfwKeyDown(key);
        // const_cast: lazy init of tracking maps on first query
        auto& self = const_cast<InputSystem&>(*this);
        if (!m_currKeys.count(key))
        {
            self.m_prevKeys[key] = false;
            self.m_currKeys[key] = curr;
        }
        return computeState(m_prevKeys.at(key), m_currKeys.at(key));
    }

    bool InputSystem::isPressed (int key) const { return keyState(key) == KeyState::Pressed;  }
    bool InputSystem::isHeld    (int key) const { return keyState(key) == KeyState::Held;     }
    bool InputSystem::isReleased(int key) const { return keyState(key) == KeyState::Released; }

    // Mouse buttons — same lazy pattern
    bool InputSystem::isMousePressed(int btn) const
    {
        bool curr = glfwMouseDown(btn);
        auto& self = const_cast<InputSystem&>(*this);
        if (!m_currMouse.count(btn))
        {
            self.m_prevMouse[btn] = false;
            self.m_currMouse[btn] = curr;
        }
        return computeState(m_prevMouse.at(btn), m_currMouse.at(btn)) == KeyState::Pressed;
    }

    bool InputSystem::isMouseHeld(int btn) const
    {
        bool curr = glfwMouseDown(btn);
        auto& self = const_cast<InputSystem&>(*this);
        if (!m_currMouse.count(btn))
        {
            self.m_prevMouse[btn] = false;
            self.m_currMouse[btn] = curr;
        }
        return computeState(m_prevMouse.at(btn), m_currMouse.at(btn)) == KeyState::Held;
    }

    bool InputSystem::isMouseReleased(int btn) const
    {
        bool curr = glfwMouseDown(btn);
        auto& self = const_cast<InputSystem&>(*this);
        if (!m_currMouse.count(btn))
        {
            self.m_prevMouse[btn] = false;
            self.m_currMouse[btn] = curr;
        }
        return computeState(m_prevMouse.at(btn), m_currMouse.at(btn)) == KeyState::Released;
    }

    // Bindings

    void InputSystem::addBinding(const std::string& name, int key, TriggerOn trigger, std::function<void()> callback)
    {
        for (auto& [n, b] : m_bindings)
        {
            if (n == name)
            {
                b = { key, trigger, InputDevice::Keyboard, std::move(callback) };
                return;
            }
        }
        m_bindings.push_back({ name, { key, trigger, InputDevice::Keyboard, std::move(callback) } });
    }

    void InputSystem::addMouseBinding(const std::string& name, int button, TriggerOn trigger, std::function<void()> callback)
    {
        for (auto& [n, b] : m_bindings)
        {
            if (n == name)
            {
                b = { button, trigger, InputDevice::Mouse, std::move(callback) };
                return;
            }
        }
        m_bindings.push_back({ name, { button, trigger, InputDevice::Mouse, std::move(callback) } });
    }

    void InputSystem::removeBinding(const std::string& name)
    {
        m_bindings.erase(
            std::remove_if(m_bindings.begin(), m_bindings.end(),
                           [&](const auto& p){ return p.first == name; }),
            m_bindings.end());
    }

    void InputSystem::clearBindings()
    {
        m_bindings.clear();
    }

    // Scroll callback

    void InputSystem::scrollCallback(GLFWwindow*, double xoff, double yoff)
    {
        if (s_scrollTarget) s_scrollTarget->onScroll(xoff, yoff);
    }

    void InputSystem::onScroll(double xoff, double yoff)
    {
        m_pendingScrollX += xoff;
        m_pendingScrollY += yoff;
    }
} // namespace gcep
