#include "script_input_bridge.hpp"

// Internals
#include <Editor/Inputs/inputs.hpp>

static gcep::InputSystem* inp(void* p) { return static_cast<gcep::InputSystem*>(p); }

static bool impl_isPressed (void* p, int k) { return inp(p)->isPressed(k);  }
static bool impl_isHeld    (void* p, int k) { return inp(p)->isHeld(k);     }
static bool impl_isReleased(void* p, int k) { return inp(p)->isReleased(k); }

static bool impl_mousePressed (void* p, int b) { return inp(p)->isMousePressed(b);  }
static bool impl_mouseHeld    (void* p, int b) { return inp(p)->isMouseHeld(b);     }
static bool impl_mouseReleased(void* p, int b) { return inp(p)->isMouseReleased(b); }

static void impl_mousePos   (void* p, double* x, double* y)
{
    *x = inp(p)->mouse().posX;
    *y = inp(p)->mouse().posY;
}
static void impl_mouseDelta (void* p, double* x, double* y)
{
    *x = inp(p)->mouse().deltaX;
    *y = inp(p)->mouse().deltaY;
}
static void impl_mouseScroll(void* p, double* x, double* y)
{
    *x = inp(p)->mouse().scrollX;
    *y = inp(p)->mouse().scrollY;
}

// Callback bridge: wraps the C function pointer + userData into a std::function
static void impl_addKeyBinding(void* p, const char* name, int key, unsigned int trigger, void(*cb)(void*), void* ud)
{
    gcep::TriggerOn t = static_cast<gcep::TriggerOn>(trigger);
    inp(p)->addBinding(name, key, t, [cb, ud]{ cb(ud); });
}
static void impl_addMouseBinding(void* p, const char* name, int key, unsigned int trigger, void(*cb)(void*), void* ud)
{
    gcep::TriggerOn t = static_cast<gcep::TriggerOn>(trigger);
    inp(p)->addMouseBinding(name, key, t, [cb, ud]{ cb(ud); });
}
static void impl_removeBinding(void* p, const char* name) { inp(p)->removeBinding(name); }
static void impl_clearBindings(void* p)                   { inp(p)->clearBindings();     }

static constexpr ScriptInputAPI API
{
    impl_isPressed, impl_isHeld, impl_isReleased,
    impl_mousePressed, impl_mouseHeld, impl_mouseReleased,
    impl_mousePos, impl_mouseDelta, impl_mouseScroll,
    impl_addKeyBinding, impl_addMouseBinding, impl_removeBinding, impl_clearBindings,
};

const ScriptInputAPI* gcep::getScriptInputAPI() { return &API; }