#pragma once

#include <memory>
#include <vector>
#include <thread>

#include "ObjectPhysicsData.hpp"

namespace gcep
{


    class PhysicsBodyDesc;
    class PhysicsWorld;

    class PhysicsManager //=> TODO: Make this a Singleton
    {
    public:
        PhysicsManager();
        ~PhysicsManager();

        PhysicsManager(const PhysicsManager&) = delete;

        std::shared_ptr<ObjectPhysicsData> createObjPhysicsData(EShapeType shapeType);

        static PhysicsManager* getInstance();

    private:

        //=> Creer le thread fixed update
        void update();

    private:
        float m_deltaTime = 1/60;
        std::vector<std::shared_ptr<ObjectPhysicsData>> m_bodiesToCreateOnBeginPlay;

        std::unique_ptr<PhysicsWorld> m_world;

        static PhysicsManager* s_instance;
    };
} // gcep