# GCEP - Entity Component System (ECS) Guide

A high-performance, Data-Oriented ECS using **Sparse Sets** and **Variadic Templates** for maximum cache efficiency.

## 1. Registry Basics

The `Registry` is the heart of the engine. It manages entity lifecycles and component storage.

```cpp
#include <Engine/Core/Entity-Component-System/headers/Registry.hpp>

gcep::Registry reg;

// Create an entity (returns a unique EntityID)
EntityID player = reg.createEntity();

// Destroy an entity (deferred until reg.update() is called)
reg.destroyEntity(player);

// Synchronize state (cleanup destroyed entities, recycle IDs)
reg.update();

```

> **Warning**: To properly recycle IDs and free memory, ensure `reg.update()` is called at the end of your main game loop.

---

## 2. Managing Components

Components are simple structs (PODs).

### Adding Components

Use `addComponent` to attach data. You can pass arguments directly to initialize the component in-place, avoiding unnecessary copies.

```cpp
struct Position { float x, y; };
struct Velocity { float vx, vy; };

// Add with default values
reg.addComponent<Position>(player); 

// Add with initial values (using Perfect Forwarding)
reg.addComponent<Velocity>(player, 5.0f, -2.0f); 

```

### Checking & Retrieving

```cpp
if (reg.hasComponent<Position>(player)) {
    auto& pos = reg.getComponent<Position>(player);
    pos.x += 10.0f;
}

// Remove a component
reg.removeComponent<Velocity>(player);

```

---

## 3. Querying Entities (The View)

The `view<Args...>` is the primary tool for iterating over entities. It is designed to be as fast as possible by following the **Smallest Pool Optimization**.

### How it works:

1. **Pivot Selection**: The View identifies which of your requested components is the "rarest" (the smallest pool).
2. **Iteration**: It iterates only over that smallest pool, skipping entities that couldn't possibly match your criteria.
3. **Filtering**: For each entity in the pivot pool, it checks for the presence of the other requested components.

```cpp
// Matches any entity that has BOTH Position AND Velocity.
// It doesn't matter if the entity has other components (like Health).
auto view = reg.view<Position, Velocity>();

for (EntityID e : view) {
    // High-speed access: Uses pointers cached during View construction
    auto& pos = view.get<Position>(e); 
    auto& vel = view.get<Velocity>(e);
    
    pos.x += vel.vx;
    pos.y += vel.vy;
}

```

---

## 4. Performance Tips

* **Order Matters (Cache Hits)**: While the View finds the smallest pool to start with, the order of `Args...` in `view<Args...>` determines the order of the remaining checks. To optimize for the CPU's short-circuit logic, place rarer components earlier in your template list.
* **View.get vs Registry.get**:
* `reg.getComponent<T>(e)`: Performs a lookup in the Registry's internal map/storage.
* `view.get<T>(e)`: Uses a **direct pointer** to the component pool cached when the view was created. **Always use the view's getter inside loops.**


* **No Branching**: The `for (EntityID e : view)` loop is a tight, predictable loop. Since the data is stored in contiguous "Dense Arrays," your CPU can prefetch component data into the L1/L2 cache effectively.

---

## 5. Example Workflow: Physics System

```cpp
void PhysicsSystem(gcep::Registry& reg, float deltaTime) {
    // We request Position, Velocity, and Gravity. 
    // The View will automatically pick the smallest of these three pools to iterate.
    auto view = reg.view<Position, Velocity, Gravity>();
    
    for (EntityID e : view) {
        auto& p = view.get<Position>(e);
        auto& v = view.get<Velocity>(e);
        auto& g = view.get<Gravity>(e);
        
        v.vy -= g.force * deltaTime;
        p.x += v.vx * deltaTime;
        p.y += v.vy * deltaTime;
    }
}

```

### Why this is fast:

1. **Zero Allocation**: No memory is allocated or freed during the loop.
2. **Minimal Indirection**: The sparse set allows $O(1)$ access to components while keeping the actual data packed tightly in memory.
3. **Short-Circuiting**: As soon as one component check fails, the View moves to the next entity immediately.


