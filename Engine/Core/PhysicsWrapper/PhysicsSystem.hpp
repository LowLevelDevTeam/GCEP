#pragma once

#include <memory>

namespace gcep
{
    enum class EShapeType
    {
        CUBE,
        SPHERE,
        CYLINDER
    };

    class PhysicsBodyDesc;
    class PhysicsWorld;

    class PhysicsSystem
    {
    public:
        PhysicsSystem();
        ~PhysicsSystem();
        PhysicsSystem(const PhysicsSystem&) = delete;

        void createBody(const PhysicsBodyDesc& desc) const;

    private:
        std::unique_ptr<PhysicsWorld> m_world;
    };
} // gcep