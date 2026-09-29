# Acorn

A simple, high-performance C++20 Entity Component System (ECS) library focused on simplicity, data locality, and automated performance tracking.

## Features

* **Fast Component Iteration**: Uses a sparse set-based design for cache-friendly, contiguous component storage.
* **Generational Handles**: Safe entity identifiers that prevent "ABA" problems by tracking reuse through generation increments.
* **Minimal API**: Clean and intuitive interface for managing entities and components without heavy boilerplate.
* **Performance First**: Integrated with Google Benchmark and GitHub Actions to ensure every commit is measured against a baseline.

## Performance Dashboard

This project uses automated benchmarking to track performance. You can view the live performance charts here:
**[https://madacorn.github.io/Acorn/dev/bench/](https://madacorn.github.io/Acorn/dev/bench/)**

### Core Metrics (Conceptual)

| Operation | Complexity | Efficiency |
| :--- | :--- | :--- |
| **Entity Creation** | $O(1)$ | High (Free-list reuse) |
| **Component Addition** | $O(1)$ | High (Sparse set insertion) |
| **Component Lookup** | $O(1)$ | Very High (Direct array access) |
| **View Iteration** | $O(N)$ | Very High (Contiguous memory) |

## Quick Start

### Basic Usage

```cpp
#include "world.hpp"

struct Position { float x, y; };
struct Velocity { float dx, dy; };

int main() {
    acorn::World world;

    // Create an entity
    auto entity = world.create_entity();

    // Add components
    world.add<Position>(entity, 0.0f, 0.0f);
    world.add<Velocity>(entity, 1.0f, 1.0f);

    // Iterate through entities with specific components
    auto view = world.view<Position, Velocity>();
    
    for (auto [e, pos, vel] : view)
    {
        pos.x += vel.dx;
        pos.y += vel.dy;
    }

    return 0;
}
```
## API

### World

| Method | Description |
| :--- | :--- |
| `create_entity()` | Creates a new entity and returns its handle. |
| `destroy_entity(e)` | Removes all components of `e` and frees its handle. Returns `false` if `e` was not alive. |
| `is_alive(e)` | `true` if `e` refers to a live entity. `false` for `Entity::null()`, destroyed entities, and stale handles whose slot was reused (different generation). Use it to validate handles stored in components. |
| `add<T>(e, args...)` | Adds (or overwrites) component `T` on `e`. |
| `remove<T>(e)` | Removes component `T` from `e`. |
| `has<T>(e)` / `get<T>(e)` / `try_get<T>(e)` | Component queries. `get` throws if absent, `try_get` returns `nullptr`. |
| `view<Ts...>()` | Iterates entities that have all of `Ts`. On a `const World`, a component type that was never used yields an empty view. |
| `view_exclude<Ts...>(Exclude<Us...>{})` | Like `view`, skipping entities that have any of `Us`. |
| `defer_remove<T>(e)` / `defer_destroy(e)` / `flush()` | Queue structural changes and apply them later. |
| `export_entities()` / `import_entities(state)` | Snapshot and restore the exact entity manager state. See [Save and load](#save-and-load). |
| `each_in_order<T>(fn)` / `restore_component<T>(e, c)` | Read a pool in dense (iteration) order and rebuild it in that order. |
| `on_destroy(fn)` | Registers a `void(World&, Entity)` listener fired when an entity is destroyed. See [Destroy events](#destroy-events). |
| `clear()` | Removes all entities and components. Does not fire destroy listeners. |

### Destroy events

Components often store handles to other entities (a reservation's holder, a task's target). Register a listener to clean those up when the referenced entity dies:

```cpp
world.on_destroy([](acorn::World& w, acorn::Entity dying) {
    for (auto [e, r] : w.view<Reservation>())
        if (r.holder == dying)
            r.holder = acorn::Entity::null();
});
```

* Listeners fire inside `destroy_entity` and inside `flush()` for `defer_destroy`, in registration order.
* They run **before** the entity's components are removed, so `get`/`try_get` on the dying entity still work.
* Calling `destroy_entity` from a listener is safe: the destruction is queued and runs (firing listeners) after the current entity is fully destroyed, in call order. `defer_*` calls made during `flush()` run in the same flush.
* Registering listeners or calling `clear()` from inside a listener is not allowed (asserted in debug builds).
* `clear()` is a bulk reset and does **not** fire listeners.
* With no listeners registered, `destroy_entity` only pays an empty-vector check.

### Save and load

Acorn does not pick a file format; it exposes hooks so a save reloads **identically**: same handles and generations (so `Entity` handles stored in components stay valid), same free-list order (so the next `create_entity()` returns the same handle), and the same iteration order for every view.

```cpp
// Save
world.flush();                                        // apply pending deferred commands
acorn::EntityState state = world.export_entities();   // { generations, free_list }
json j;
j["generations"] = state.generations;
j["free_list"] = state.free_list;
world.each_in_order<Position>([&](acorn::Entity e, const Position& p) {
    j["Position"].push_back({e.index, e.generation, p.x, p.y});
});

// Load (on a fresh World, or after world.clear())
acorn::EntityState loaded{j["generations"].get<std::vector<uint32_t>>(),
                          j["free_list"].get<std::vector<uint32_t>>()};
world.import_entities(loaded);
for (const auto& row : j["Position"]) {                // same order as saved
    acorn::Entity e{row[0].get<uint32_t>(), row[1].get<uint32_t>()};
    world.restore_component<Position>(e, Position{row[2].get<float>(), row[3].get<float>()});
}
```

* `import_entities` throws `std::logic_error` if the world has any entity slots or components (call `clear()` first) and `std::invalid_argument` for a malformed free list.
* `restore_component` appends in call order and throws `std::logic_error` if the entity is not alive or already has the component.
* Restore every component type you saved, each in the order `each_in_order` produced it.

## Core Components
* **World**: The central container managing the EntityManager and ComponentPools.

* **EntityManager**: Handles entity allocation, destruction, and generational tracking.

* **ComponentPool**: Implements a sparse set to store component data contiguously in memory.

* **View**: Provides an efficient way to iterate over entities that possess a specific set of components.

## Testing and Benchmarks
### Running Tests

To build and run the unit tests:

```Bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Running Benchmarks

To evaluate performance locally using Google Benchmark:
```Bash
cmake -S . -B build -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acorn_bench
```

## License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/madacorn/Acorn/blob/main/LICENSE) file for details.