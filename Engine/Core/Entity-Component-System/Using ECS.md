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

---
### Warning: If you want to destroy entities, make sure Registry Update is called at the end of the game loop.

## 2. Managing Components

Components are simple structs. Use `addComponent` to attach data.

### Adding Components (In-place Construction)

You can pass arguments directly to initialize the component without copies.

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

## 3. Querying Entities (Views)

Views are the primary way to iterate over entities. They use **Smallest Pool Optimization**: the engine identifies the rarest component in your query to minimize the number of checks.

### `partialView<Args...>()`

- Returns entities that have **at least** the specified components. They may have others. This is your "bread and butter" for systems (Physics, Rendering).
-  Example: partialView<Pos, Vel> matches entities with {Position, Velocity} but also {Position, Velocity, Health, Name}.
```cpp
// Logic: (Has Position AND Has Velocity)
auto view = reg.partialView<Position, Velocity>();

for (EntityID e : view) {
    auto& pos = view.get<Position>(e); // Fast access via cached pool
    auto& vel = view.get<Velocity>(e);
    pos.x += vel.vx;
}

```

### `exactView<Args...>()`

Returns entities that have **exactly and only** the specified components. If an entity has an extra component not listed in the template, it is ignored. Useful for very specific logic or strict state machines.
Example: exactView<Position, Health> matches entities with {Position, Health} but ignores entities with {Position, Health, Velocity}.

```cpp
// Logic: (Has Position ) AND (Has Health) And (Has No other Components)
auto view = reg.exactView<Position, Health>(); 

for (EntityID e : view) {
    // This entity is purely a "Position" and "Health" object
}

```

---

### Which one to use?

### `partialView<Args...>` (Behavioral Logic)
- **Use `partialView`**: Use this 95% of the time. It allows your systems to work even if entities are "decorated" with extra data (like a Name, Tag, or AIPriority).


- Example: A PhysicsSystem only cares about Position and Velocity. It shouldn't stop working just because you added a Mesh or a SoundEmitter to the entity.

### `exactView<Args...>` (Archetype Identification)
- **Use this to find specific objects.** It filters out any entity that has been "polluted" by extra components.

- Example: If a `Player` is strictly defined as `Health + Pos + Vel`, using exactView ensures you don't accidentally grab an `NPC` that has `Health + Pos + Vel` plus an AIControl component.
---

## 4. Performance Tips

* **Batch Operations**: Call `reg.update()` once per frame. This clears the destruction queue and recycles IDs.
* **View Getters**: Inside a loop, **always** use `view.get<T>(entity)`.
* `reg.getComponent<T>` performs a hash/map lookup every time.
* `view.get<T>` uses a pointer directly cached during the view's construction.


* **Perfect Forwarding**: Use the multi-argument `addComponent` to build components directly in the pool, avoiding temporary objects.

### Example Workflow: A Simple Spawner & System

```cpp
// 1. Spawning with data "on the fly"
void SpawnPlayer(Registry& reg, float x, float y) {
    EntityID player = reg.createEntity();
    
    // In-place construction: no temporary Pos/Vel objects created
    reg.addComponent<Position>(player, x, y); 
    reg.addComponent<Velocity>(player, 0.0f, 0.0f);
    reg.addComponent<Health>(player, 100);
}

// 2. Systematic Update (The "Bulk" way)
void PhysicsSystem(Registry& reg) {
    // We use partialView because we don't care if the entity has 
    // extra components like 'Name' or 'Mesh'.
    auto view = reg.partialView<Position, Velocity, Gravity>();
    
    for (EntityID e : view) {
        // High-speed access via cached pointers
        auto& p = view.get<Position>(e);
        auto& v = view.get<Velocity>(e);
        auto& g = view.get<Gravity>(e);
        
        v.vy -= g.force * 0.016f;
        p.x += v.vx;
        p.y += v.vy;
    }
}

```

---

### Why this is fast:

1. **Contiguous Memory**: `view.get<T>` accesses the `m_components` vector linearly. Your CPU's prefetcher will load the next components into the L1 cache before you even ask for them.
2. **No Branching**: The `for (auto e : view)` loop is a tight, predictable loop that the compiler can easily optimize (or even SIMD-vectorize).
3. **Zero Allocation**: Aside from the initial `createEntity`, no memory allocation happens during the physics update.



