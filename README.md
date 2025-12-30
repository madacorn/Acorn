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
    for (auto e : view) {
        auto& pos = world.get<Position>(e);
        auto& vel = world.get<Velocity>(e);

        pos.x += vel.dx;
        pos.y += vel.dy;
    }

    return 0;
}
```
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