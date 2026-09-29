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
| `clear()` | Removes all entities and components. |

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