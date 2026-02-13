#include "PhysicsManager.hpp"

#include <memory>

#include "PhysicalWorld.hpp"

namespace gcep
{
    PhysicsManager::PhysicsManager()
    {
        m_world = std::make_unique<PhysicsWorld>();
        for (auto& obj : m_bodiesToCreateOnBeginPlay) {
            m_world->createBody(obj);
        }
    }

    PhysicsManager::~PhysicsManager()
    {
        m_world.reset();
    }

    std::shared_ptr<ObjectPhysicsData> PhysicsManager::createObjPhysicsData(EShapeType shapeType) {
        std::shared_ptr<ObjectPhysicsData> data = std::make_shared<ObjectPhysicsData>(shapeType);
        m_bodiesToCreateOnBeginPlay.push_back(data);
        return data;
    }

    void PhysicsManager::update()
    {
        m_world->step(m_deltaTime);
    }

    PhysicsManager* PhysicsManager::getInstance()
    {

        if (!s_instance)
        {
            s_instance = new PhysicsManager();
        }

        return s_instance;
    }

    PhysicsManager* PhysicsManager::s_instance = nullptr;
} // gcep