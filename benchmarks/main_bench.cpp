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

    auto view = world.view<Position, Velocity>();

    for (auto _ : state)
    {
        view.each(
            [](acorn::Entity e, Position& pos, Velocity& vel)
            {
                pos.x += vel.dx;
                benchmark::DoNotOptimize(pos);
            });
    }
    state.SetItemsProcessed(state.iterations() * (entity_count / 2));
}

BENCHMARK(BM_ViewIteration)->Range(1000, 10000);

static void BM_ViewIteration_SparseMatch(benchmark::State& state)
{
    acorn::World world;
    const size_t entity_count = state.range(0);

    for (size_t i = 0; i < entity_count; ++i)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.0f, 1.0f);

        if (i % 100 == 0)
        {
            world.add<Velocity>(e, 0.1f, 0.1f);
        }
    }

    for (auto _ : state)
    {
        auto view = world.view<Position, Velocity>();
        for (auto [e, pos, vel] : view)
        {
            benchmark::DoNotOptimize(pos);
        }
    }
}

BENCHMARK(BM_ViewIteration_SparseMatch)->Range(1000, 10000);

static void BM_ViewIteration_SingleComponent(benchmark::State& state)
{
    acorn::World world;
    const size_t entity_count = state.range(0);
    for (size_t i = 0; i < entity_count; ++i)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.0f, 1.0f);
    }

    for (auto _ : state)
    {
        auto view = world.view<Position>();
        for (auto [e, pos] : view)
        {
            benchmark::DoNotOptimize(pos);
        }
    }
    state.SetItemsProcessed(state.iterations() * entity_count);
}

BENCHMARK(BM_ViewIteration_SingleComponent)->Range(1000, 10000);

struct PlayerTag
{
};

struct FrozenTag
{
};

static void BM_ViewIteration_NoExclude(benchmark::State& state)
{
    acorn::World world;
    const size_t count = state.range(0);

    for (size_t i = 0; i < count; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.f, 1.f);
        world.add<Velocity>(e, 0.1f, 0.1f);
    }

    for (auto _ : state)
    {
        auto view = world.view<Position, Velocity>();
        view.each(
            [](acorn::Entity, Position& pos, Velocity& vel)
            {
                pos.x += vel.dx;
                benchmark::DoNotOptimize(pos);
            });
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(BM_ViewIteration_NoExclude)->Range(1000, 100000);

static void BM_ExcludeView_NoneExcluded(benchmark::State& state)
{
    acorn::World world;
    const size_t count = state.range(0);

    for (size_t i = 0; i < count; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.f, 1.f);
        world.add<Velocity>(e, 0.1f, 0.1f);
        // no PlayerTag on anything — exclude pool is empty
    }

    for (auto _ : state)
    {
        auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<PlayerTag>{});
        view.each(
            [](acorn::Entity, Position& pos, Velocity& vel)
            {
                pos.x += vel.dx;
                benchmark::DoNotOptimize(pos);
            });
    }

    state.SetItemsProcessed(state.iterations() * count);
}

BENCHMARK(BM_ExcludeView_NoneExcluded)->Range(1000, 100000);

static void BM_ExcludeView_HalfExcluded(benchmark::State& state)
{
    acorn::World world;
    const size_t count = state.range(0);

    for (size_t i = 0; i < count; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, 1.f, 1.f);
        world.add<Velocity>(e, 0.1f, 0.1f);
        if (i % 2 == 0)
            world.add<PlayerTag>(e);
    }

    for (auto _ : state)
    {
        auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<PlayerTag>{});
        view.each([](acorn::Entity, Position& pos, Velocity& vel)
                  { benchmark::DoNotOptimize(pos); });
    }

    state.SetItemsProcessed(state.iterations() * (count / 2));
}

BENCHMARK(BM_ExcludeView_HalfExcluded)->Range(1000, 100000);

static void BM_ExcludeView_OneExcluded(benchmark::State& state)
{
    acorn::World world;
    const size_t count = state.range(0);

    // one player
    auto player = world.create_entity();
    world.add<Position>(player, 0.f, 0.f);
    world.add<Velocity>(player, 0.f, 0.f);
    world.add<PlayerTag>(player);

    // rest are creatures
    for (size_t i = 1; i < count; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, (float)i);
        world.add<Velocity>(e, 0.1f, 0.1f);
    }

    for (auto _ : state)
    {
        auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<PlayerTag>{});
        view.each(
            [](acorn::Entity, Position& pos, Velocity& vel)
            {
                pos.x += vel.dx;
                benchmark::DoNotOptimize(pos);
            });
    }

    state.SetItemsProcessed(state.iterations() * (count - 1));
}

BENCHMARK(BM_ExcludeView_OneExcluded)->Range(1000, 100000);

static void BM_ExcludeView_MultipleExcludeTags(benchmark::State& state)
{
    acorn::World world;
    const size_t count = state.range(0);

    for (size_t i = 0; i < count; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, (float)i);
        world.add<Velocity>(e, 0.1f, 0.1f);
        if (i % 10 == 0)
            world.add<PlayerTag>(e);
        if (i % 15 == 0)
            world.add<FrozenTag>(e);
    }

    for (auto _ : state)
    {
        auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<PlayerTag, FrozenTag>{});
        view.each([](acorn::Entity, Position& pos, Velocity& vel)
                  { benchmark::DoNotOptimize(pos); });
    }
}

BENCHMARK(BM_ExcludeView_MultipleExcludeTags)->Range(1000, 100000);

BENCHMARK_MAIN();