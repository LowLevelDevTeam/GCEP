// PlayerController.cpp
#include "script_api.hpp"
#include "script_object.hpp"
#include <cmath>
#include <format>
#include <iostream>

static constexpr float k_moveSpeed      = 6.f;
static constexpr float k_sprintMult     = 2.2f;
static constexpr float k_mouseSens      = 0.12f;
static constexpr float k_scrollZoomMult = 0.8f;
static constexpr float k_bobFreq        = 8.f;
static constexpr float k_bobAmp         = 0.06f;
static constexpr float k_pi             = 3.14159265f;

static float lerp (float a, float b, float t)          { return a + (b - a) * t; }
static float clamp(float v, float lo, float hi)        { return v < lo ? lo : v > hi ? hi : v; }
static float toRad(float deg)                          { return deg * (k_pi / 180.f); }
static float sinf_ (float x)                           { return std::sin(x); }

struct PlayerController
{
    float yaw        = 0.f;
    float pitch      = 0.f;
    float bobTimer   = 0.f;
    float bobY       = 0.f;
    float targetFov  = 60.f;
    float currentFov = 60.f;
    bool  sprinting  = false;
    int   frame      = 0;

    void onStart(const ScriptContext* ctx)
    {
        GameObject go(ctx);
        Input      input(ctx);

        go.setTag("Player");

        if (auto cam = go.getOrAddComponent<ScriptCameraComponent, ScriptComponentType::Camera>())
        {
            currentFov = cam->fovYDeg;
            targetFov  = currentFov;
        }

        input.bindKey("sprintOn",  gcep::Key::LeftShift, gcep::TriggerOn::Pressed,
            [](void* ud){ static_cast<PlayerController*>(ud)->sprinting = true;  }, this);
        input.bindKey("sprintOff", gcep::Key::LeftShift, gcep::TriggerOn::Released,
            [](void* ud){ static_cast<PlayerController*>(ud)->sprinting = false; }, this);
    }

    void onUpdate(const ScriptContext* ctx)
    {
        ++frame;

        GameObject  go(ctx);
        Input       input(ctx);
        const float dt    = ctx->deltaTime;
        const float speed = k_moveSpeed * (sprinting ? k_sprintMult : 1.f);

        auto tf = GET_TRANSFORM(go);
        if (!tf)
        {
            return;
        }

        // ── Mouse look ────────────────────────────────────────────────────────
        // Always active — editor camera is frozen during simulation anyway
        {
            auto delta = input.mouseDelta();
            yaw   += static_cast<float>(delta.x) * k_mouseSens;
            pitch -= static_cast<float>(delta.y) * k_mouseSens;
            pitch  = clamp(pitch, -89.f, 89.f);
            if (yaw >  180.f) yaw -= 360.f;
            if (yaw < -180.f) yaw += 360.f;

            tf->eulerRadians.x = toRad(pitch);
            tf->eulerRadians.y = toRad(yaw);
        }

        // ── WASD movement (yaw-relative, no pitch on movement) ────────────────
        {
            const float yr   = toRad(yaw);
            const float fwdX =  sinf_(yr),  fwdZ = -std::cos(yr);
            const float rgtX =  std::cos(yr), rgtZ =  sinf_(yr);

            float mx = 0.f, mz = 0.f;
            if (input.getKey(gcep::Key::W)) { mx += fwdX; mz += fwdZ; }
            if (input.getKey(gcep::Key::S)) { mx -= fwdX; mz -= fwdZ; }
            if (input.getKey(gcep::Key::D)) { mx += rgtX; mz += rgtZ; }
            if (input.getKey(gcep::Key::A)) { mx -= rgtX; mz -= rgtZ; }
            if (input.getKey(gcep::Key::E)) tf->position.y += speed * dt;
            if (input.getKey(gcep::Key::Q)) tf->position.y -= speed * dt;

            // Normalise diagonal
            const float len = std::sqrt(mx * mx + mz * mz);
            if (len > 1.f) { mx /= len; mz /= len; }

            tf->position.x += mx * speed * dt;
            tf->position.z += mz * speed * dt;

            // ── Head bob ──────────────────────────────────────────────────────
            const bool moving = len > 0.001f;
            if (moving)
            {
                bobTimer += dt * k_bobFreq;
                bobY      = sinf_(bobTimer) * k_bobAmp;
            }
            else
            {
                bobTimer = 0.f;
                bobY     = lerp(bobY, 0.f, dt * 10.f);
            }
            tf->position.y += bobY;
        }

        // ── Scroll zoom ───────────────────────────────────────────────────────
        {
            const float scroll = input.mouseScroll().y;
            if (scroll != 0.f)
                targetFov = clamp(targetFov - scroll * k_scrollZoomMult * 5.f, 20.f, 110.f);

            currentFov = lerp(currentFov, targetFov, dt * 12.f);
            if (auto cam = GET_CAMERA(go))
                cam->fovYDeg = currentFov;
        }

        // ── Point light pulse ─────────────────────────────────────────────────
        if (auto light = GET_POINT_LIGHT(go))
            light->intensity = 1.f + 0.15f * sinf_(static_cast<float>(frame) * 0.08f);

        // ── Debug reset ───────────────────────────────────────────────────────
        if (input.getKeyDown(gcep::Key::R))
        {
            tf->position = { 0.f, tf->position.y, 0.f };
            yaw          = 0.f;
            pitch        = 0.f;
            currentFov   = 60.f;
            targetFov    = 60.f;
        }
    }

    void onEnd(const ScriptContext* ctx)
    {
        Input(ctx).clearAllBindings();
    }
};

DECLARE_SCRIPT(PlayerController)