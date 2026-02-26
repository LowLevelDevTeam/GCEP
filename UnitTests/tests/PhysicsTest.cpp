#include <gtest/gtest.h>

#include "Engine/Core/PhysicsWrapper/physics_shape.hpp"
#include "Engine/Core/PhysicsWrapper/physics_world.hpp"
#include "Engine/Core/PhysicsWrapper/physics_system.hpp"
#include <Engine/Core/Entity-Component-System/headers/registry.hpp>

using namespace gcep;

// PHYSICS WORLD

class PhysicsWorldFixture : public ::testing::Test
{
protected:
    PhysicsWorld world;
};

TEST_F(PhysicsWorldFixture, CreateStaticBody)
{
    TransformComponent transform;
    PhysicsComponent physics;

    physics.motionType = EMotionType::STATIC;
    physics.shapeType = EShapeType::CUBE;

    JPH::BodyID id;

    world.createBody(transform, physics, id);

    EXPECT_FALSE(id.IsInvalid());
}

TEST_F(PhysicsWorldFixture, DynamicBodyFalls)
{
    TransformComponent transform;
    transform.position = {0.f, 10.f, 0.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::DYNAMIC;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    float initialY = transform.position.y;

    world.step(1.0f); // simulate 1 second

    auto& bodyInterface = world.m_physicsSystem->GetBodyInterface();
    JPH::RVec3 pos = bodyInterface.GetPosition(id);

    EXPECT_LT(pos.GetY(), initialY);
}

TEST_F(PhysicsWorldFixture, DestroyBody)
{
    TransformComponent transform;
    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    EXPECT_FALSE(id.IsInvalid());

    world.destroyBody(id);

    // If no crash → success
    SUCCEED();
}

// PHYSICS SHAPE

TEST(PhysicsShapeTest, CreateBoxValid)
{
    auto shape = PhysicsShape::createBox({1.f, 1.f, 1.f});

    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->getType(), EPhysicsShapeType::BOX);
    EXPECT_TRUE(shape->getJoltShape() != nullptr);
}

TEST(PhysicsShapeTest, CreateSphereValid)
{
    auto shape = PhysicsShape::createSphere(1.0f);

    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->getType(), EPhysicsShapeType::SPHERE);
}

TEST(PhysicsShapeTest, CreateCapsuleValid)
{
    auto shape = PhysicsShape::createCapsule(2.0f, 0.5f);

    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->getType(), EPhysicsShapeType::CAPSULE);
}

TEST(PhysicsShapeTest, CreateScaledShape)
{
    auto base = PhysicsShape::createBox({1.f,1.f,1.f});
    ASSERT_NE(base, nullptr);

    auto scaled = PhysicsShape::createScaled(base, {2.f,2.f,2.f});
    ASSERT_NE(scaled, nullptr);

    EXPECT_EQ(scaled->getType(), EPhysicsShapeType::BOX);
}

// Raycast

TEST_F(PhysicsWorldFixture, RaycastHitsBody)
{
    TransformComponent transform;
    transform.position = {0.f, 0.f, 0.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::STATIC;
    physics.shapeType = EShapeType::CUBE;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -1.f, 0.f},
        500.f
    );

    EXPECT_TRUE(hit.hasHit);
    EXPECT_FALSE(hit.bodyID.IsInvalid());
}

TEST(PhysicsSystemTest, StartSimulationCreatesBodies)
{
    auto& system = PhysicsSystem::getInstance();

    Registry registry;
    system.reg = registry;
    auto entity1 = system.reg.createEntity();

    TransformComponent* tComp= &system.reg.addComponent<TransformComponent>(entity1);
    PhysicsComponent* pComp =  &system.reg.addComponent<PhysicsComponent>(entity1);

    system.startSimulation();

    auto& pc = system.reg.getComponent<PhysicsComponent>(entity1);

    EXPECT_FALSE(pc.m_bodyIDRef.IsInvalid());
}