#include <gtest/gtest.h>

#include "Engine/Core/PhysicsWrapper/physics_shape.hpp"
#include "Engine/Core/PhysicsWrapper/physics_world.hpp"
#include "Engine/Core/PhysicsWrapper/physics_system.hpp"
#include <Engine/Core/ECS/headers/registry.hpp>

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
/*
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

TEST_F(PhysicsWorldFixture, RaycastMiss)
{
    RaycastHit hit = world.raycast(
        {0.f,0.f,0.f},
        {1.f,0.f,0.f},
        10.f
    );

    EXPECT_FALSE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastHitsStaticCube)
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
    EXPECT_GT(hit.distance, 0.f);
}

TEST_F(PhysicsWorldFixture, RaycastRespectsMaxDistance)
{
    TransformComponent transform;
    transform.position = {0.f, 0.f, 0.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::STATIC;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -1.f, 0.f},
        50.f   // trop court
    );

    EXPECT_FALSE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastReturnsClosestBody)
{
    TransformComponent t1;
    t1.position = {0.f, 0.f, 0.f};

    TransformComponent t2;
    t2.position = {0.f, -200.f, 0.f};

    PhysicsComponent p1;
    PhysicsComponent p2;

    JPH::BodyID id1, id2;

    world.createBody(t1, p1, id1);
    world.createBody(t2, p2, id2);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 300.f, 0.f},
        {0.f, -1.f, 0.f},
        1000.f
    );

    EXPECT_TRUE(hit.hasHit);
    EXPECT_EQ(hit.bodyID, id1); // le plus proche
}

TEST_F(PhysicsWorldFixture, RaycastReturnsCorrectNormal)
{
    TransformComponent transform;
    transform.position = {0.f, 0.f, 0.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -1.f, 0.f},
        500.f
    );

    EXPECT_TRUE(hit.hasHit);

    // normale doit pointer vers le haut
    EXPECT_NEAR(hit.normal.y, 1.f, 0.1f);
}

TEST_F(PhysicsWorldFixture, RaycastWithNonNormalizedDirection)
{
    TransformComponent transform;
    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -10.f, 0.f},  // non normalisée
        500.f
    );

    EXPECT_TRUE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastFromInsideObject)
{
    TransformComponent transform;
    transform.position = {0.f, 0.f, 0.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 0.f, 0.f},   // inside
        {1.f, 0.f, 0.f},
        100.f
    );

    EXPECT_TRUE(hit.hasHit);
    EXPECT_GE(hit.distance, 0.f); // <= important
}
TEST_F(PhysicsWorldFixture, HorizontalRaycast)
{
    TransformComponent transform;
    transform.position = {0.f, 0.f, 0.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {-200.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        500.f
    );

    EXPECT_TRUE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastOnDynamicBody)
{
    TransformComponent transform;
    transform.position = {0.f, 100.f, 0.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::DYNAMIC;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(1.0f); // il tombe

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -1.f, 0.f},
        500.f
    );

    EXPECT_TRUE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastAfterBodyDestroyed)
{
    TransformComponent transform;
    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.destroyBody(id);

    RaycastHit hit = world.raycast(
        {0.f, 200.f, 0.f},
        {0.f, -1.f, 0.f},
        500.f
    );

    EXPECT_FALSE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, RaycastScaledObject)
{
    TransformComponent transform;
    transform.scale = {3.f, 3.f, 3.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    RaycastHit hit = world.raycast(
        {0.f, 500.f, 0.f},
        {0.f, -1.f, 0.f},
        1000.f
    );

    EXPECT_TRUE(hit.hasHit);
}

TEST_F(PhysicsWorldFixture, MultipleRaycastsStability)
{
    TransformComponent transform;
    transform.scale = {100.f, 100.f, 100.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    world.step(0.1f);

    for (float x = -90.f; x <= 90.f; x += 10.f)
    {
        RaycastHit hit = world.raycast(
            {x, 200.f, 0.f},
            {0.f, -1.f, 0.f},
            500.f
        );

        EXPECT_TRUE(hit.hasHit) << "Failed at x=" << x;
    }
}

// Many bodies
TEST_F(PhysicsWorldFixture, ManyBodiesSimulation)
{
    constexpr int count = 1000;

    for (int i = 0; i < count; ++i)
    {
        TransformComponent transform;
        transform.position = {0.f, float(i * 10), 0.f};

        PhysicsComponent physics;
        physics.motionType = EMotionType::DYNAMIC;

        JPH::BodyID id;
        world.createBody(transform, physics, id);
    }

    world.step(0.016f);

    SUCCEED();
}

// Zero scale
TEST_F(PhysicsWorldFixture, ZeroScaleBody)
{
    TransformComponent transform;
    transform.scale = {0.f,0.f,0.f};

    PhysicsComponent physics;

    JPH::BodyID id;
    world.createBody(transform, physics, id);

    // Should not crash
    SUCCEED();
}

// Simulation
TEST(PhysicsSystemTest, StartSimulationCreatesBodies)
{
    auto& system = PhysicsSystem::getInstance();

    EntityID entity = system.reg.createEntity();

    auto& transform = system.reg.addComponent<TransformComponent>(entity);
    auto& physicsComponent = system.reg.addComponent<PhysicsComponent>(entity);

    system.startSimulation();

    EXPECT_FALSE(physicsComponent.m_bodyIDRef.IsInvalid());
}

TEST(PhysicsSystemTest, SyncPhysicsToTransform)
{
    auto& system = PhysicsSystem::getInstance();

    EntityID entity = system.reg.createEntity();

    TransformComponent transform;
    transform.position = {0.f, 10.f, 0.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::DYNAMIC;

    auto& tc = system.reg.addComponent<TransformComponent>(entity, transform);
    auto& pc = system.reg.addComponent<PhysicsComponent>(entity, physics);

    system.startSimulation();

    system.update(1.0f);

    auto& updatedTransform =
        system.reg.getComponent<TransformComponent>(entity);

    EXPECT_LT(updatedTransform.position.y, 10.f);
}

TEST(PhysicsSystemTest, SyncTransformToPhysicsKinematic)
{
    auto& system = PhysicsSystem::getInstance();

    EntityID entity = system.reg.createEntity();

    TransformComponent transform;
    transform.position = {5.f, 5.f, 5.f};

    PhysicsComponent physics;
    physics.motionType = EMotionType::KINEMATIC;

    auto& tc = system.reg.addComponent<TransformComponent>(entity, transform);
    auto& pc = system.reg.addComponent<PhysicsComponent>(entity, physics);

    system.startSimulation();
    system.update(0.1f);

    // If no crash → pass
    SUCCEED();
}*/