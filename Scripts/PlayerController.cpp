#include "script_api.hpp"
#include "script_object.hpp"

#include <cmath>

static constexpr float k_speed  = 6.f;
static constexpr float k_sprint = 2.2f;
static constexpr float k_sens   = 0.12f;
static constexpr float k_pi     = 3.14159265f;

static float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float toRad(float d) { return d * (k_pi / 180.f); }

struct PlayerController
{
    bool sprint = false;

    void onStart(const ScriptContext* ctx)
    {
        GameObject  go(ctx);
        go.setTag("Player");
        auto cam = go.getOrAddComponent<ScriptCameraComponent, ScriptComponentType::Camera>();
        cam.setPosition(GET_TRANSFORM(go)->position);
        Input input(ctx);
        input.bindKey("sprintOn",  gcep::Key::LeftShift, gcep::TriggerOn::Pressed,
            [](void* ud){ static_cast<PlayerController*>(ud)->sprint = true;  }, this);
        input.bindKey("sprintOff", gcep::Key::LeftShift, gcep::TriggerOn::Released,
            [](void* ud){ static_cast<PlayerController*>(ud)->sprint = false; }, this);
    }

    void onUpdate(const ScriptContext* ctx)
    {
        GameObject  go(ctx);
        Input       input(ctx);
        const float dt    = ctx->deltaTime;
        const float speed = k_speed * (sprint ? k_sprint : 1.f);

        auto tf  = GET_TRANSFORM(go);
        auto cam = GET_CAMERA(go);
        if (!tf || !cam || !input.valid()) return;

        // Scroll to change FOV
        const auto scroll = input.mouseScroll();
        cam.setFovY(clamp(cam.getFovY() - scroll.y * 2.f, 10.f, 120.f));

        // Mouse look
        const auto delta = input.mouseDelta();
        cam.addYaw  (-delta.x * k_sens);
        cam.addPitch(-delta.y * k_sens);

        // Movement
        const float yawRad = toRad(cam.getYaw());
        const float fwdX =  std::cos(yawRad);
        const float fwdY =  std::sin(yawRad);
        const float rgtX =  fwdY;
        const float rgtY = -fwdX;

        float mx = 0.f, my = 0.f;
        if (input.getKey(gcep::Key::W)) { mx += fwdX; my += fwdY; }
        if (input.getKey(gcep::Key::S)) { mx -= fwdX; my -= fwdY; }
        if (input.getKey(gcep::Key::A)) { mx -= rgtX; my -= rgtY; }
        if (input.getKey(gcep::Key::D)) { mx += rgtX; my += rgtY; }

        const float len = std::sqrt(mx * mx + my * my);
        if (len > 1.f) { mx /= len; my /= len; }

        tf->position.x += mx * speed * dt;
        tf->position.y += my * speed * dt;

        if (input.getKey(gcep::Key::E)) tf->position.z += speed * dt;
        if (input.getKey(gcep::Key::Q)) tf->position.z -= speed * dt;

        // Reset
        if (input.getKeyDown(gcep::Key::R))
        {
            tf->position = { 0.f, 0.f, tf->position.z };
            cam.setPitch(0.f);
            cam.setYaw(0.f);
        }

        cam.setPosition(tf->position);
    }

    void onEnd(const ScriptContext* ctx)
    {
        Input(ctx).clearAllBindings();
    }
};

DECLARE_SCRIPT(PlayerController)