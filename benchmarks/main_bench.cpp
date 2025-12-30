#include <benchmark/benchmark.h>

#include "world.hpp"

struct Position
{
    float x, y;
};

struct Velocity
{
    float dx, dy;
};

static void BM_EntityCreation(benchmark::State& state)
{
    acorn::World world;
    for (auto _ : state)
    {
        auto e = world.create_entity();
        benchmark::DoNotOptimize(e);
    }
}

BENCHMARK(BM_EntityCreation);

static void BM_ComponentAdd(benchmark::State& state)
{
    acorn::World world;
    auto e = world.create_entity();
    for (auto _ : state)
    {
        world.add<Position>(e, 1.0f, 2.0f);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_ComponentAdd);

static void BM_ViewIteration(benchmark::State& state)
{
    acorn::World world;
    const size_t entity_count = state.range(0);

    for (size_t i = 0; i < entity_count; ++i)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.0f, 1.0f);
        if (i % 2 == 0)
        {
            world.add<Velocity>(e, 0.1f, 0.1f);
        }
    }

    for (auto _ : state)
    {
        auto view = world.view<Position, Velocity>();
        for (auto e : view)
        {
            auto& pos = world.get<Position>(e);
            auto& vel = world.get<Velocity>(e);
            pos.x += vel.dx;
            pos.y += vel.dy;
            benchmark::DoNotOptimize(pos);
        }
    }
    state.SetItemsProcessed(state.iterations() * (entity_count / 2));
}

// Run for 1,000 and 10,000 entities
BENCHMARK(BM_ViewIteration)->Range(1000, 10000);

BENCHMARK_MAIN();