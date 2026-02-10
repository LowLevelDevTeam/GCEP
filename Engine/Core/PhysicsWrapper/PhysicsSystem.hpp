#pragma once

#include <memory>

namespace gcep
{
    class PhysicsBodyDesc;
    class PhysicsWorld;

    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();
        PhysicsSystem(const PhysicsSystem&) = delete;

        void createBody(const PhysicsBodyDesc& desc);

    private:
        std::unique_ptr<PhysicsWorld> m_world;
    };
} // gcep