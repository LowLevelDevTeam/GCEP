#pragma once

#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>

// STL
#include <mutex>
#include <vector>

struct RawContactEvent
{
    JPH::BodyID body1;
    JPH::BodyID body2;
    float contactX, contactY, contactZ;
    float normalX,  normalY,  normalZ;
};

struct RawContactRemovedEvent
{
    JPH::BodyID body1;
    JPH::BodyID body2;
};

class PhysicsContactListener : public JPH::ContactListener
{
public:
    JPH::ValidateResult OnContactValidate(
        const JPH::Body &inBody1,
        const JPH::Body &inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult &inCollisionResult
    ) override
    {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const JPH::Body &inBody1,
        const JPH::Body &inBody2,
        const JPH::ContactManifold &inManifold,
        JPH::ContactSettings &ioSettings
    ) override
    {
        RawContactEvent e;
        e.body1 = inBody1.GetID();
        e.body2 = inBody2.GetID();

        if (!inManifold.mRelativeContactPointsOn1.empty())
        {
            JPH::Vec3 cp = inManifold.GetWorldSpaceContactPointOn1(0);
            e.contactX = cp.GetX(); e.contactY = cp.GetY(); e.contactZ = cp.GetZ();
        }
        else { e.contactX = e.contactY = e.contactZ = 0.f; }

        e.normalX = inManifold.mWorldSpaceNormal.GetX();
        e.normalY = inManifold.mWorldSpaceNormal.GetY();
        e.normalZ = inManifold.mWorldSpaceNormal.GetZ();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_enterEvents.push_back(e);
    }

    void OnContactPersisted(
        const JPH::Body &inBody1,
        const JPH::Body &inBody2,
        const JPH::ContactManifold &inManifold,
        JPH::ContactSettings &ioSettings
    ) override {}

    void OnContactRemoved(
        const JPH::SubShapeIDPair &inSubShapePair
    ) override
    {
        RawContactRemovedEvent e;
        e.body1 = inSubShapePair.GetBody1ID();
        e.body2 = inSubShapePair.GetBody2ID();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_exitEvents.push_back(e);
    }

    std::vector<RawContactEvent> flushEnterEvents()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_enterEvents);
    }

    std::vector<RawContactRemovedEvent> flushExitEvents()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_exitEvents);
    }

private:
    std::mutex m_mutex;
    std::vector<RawContactEvent>        m_enterEvents;
    std::vector<RawContactRemovedEvent> m_exitEvents;
};