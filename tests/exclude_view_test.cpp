#include <gtest/gtest.h>

#include "world.hpp"

struct Position
{
    float x, y;
};

struct Velocity
{
    float dx, dy;
};

struct PlayerTag
{
};

struct FrozenTag
{
};

struct BossTag
{
};

TEST(ExcludeViewTest, ExcludesEntitiesWithTag)
{
    acorn::World world;

    auto player = world.create_entity();
    auto creature = world.create_entity();

    world.add<Position>(player, 0.f, 0.f);
    world.add<Position>(creature, 5.f, 5.f);
    world.add<PlayerTag>(player);

    size_t count = 0;
    auto view = world.view_exclude<Position>(acorn::Exclude<PlayerTag>{});
    view.each(
        [&](acorn::Entity e, Position& pos)
        {
            EXPECT_EQ(e, creature);
            EXPECT_FLOAT_EQ(pos.x, 5.f);
            count++;
        });

    EXPECT_EQ(count, 1);
}

TEST(ExcludeViewTest, EmptyWorldReturnsNothing)
{
    acorn::World world;
    world.create_entity();  // entity with no components

    int count = 0;
    auto view = world.view_exclude<Position>(acorn::Exclude<PlayerTag>{});
    view.each([&](acorn::Entity, Position&) { count++; });

    EXPECT_EQ(count, 0);
}

TEST(ExcludeViewTest, NoExcludedEntitiesPresent_AllReturned)
{
    acorn::World world;

    for (int i = 0; i < 3; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, (float)i);
        world.add<Velocity>(e, 1.f, 1.f);
    }

    size_t count = 0;
    auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<PlayerTag>{});
    view.each([&](acorn::Entity, Position&, Velocity&) { count++; });

    EXPECT_EQ(count, 3);
}

TEST(ExcludeViewTest, MultipleExcludeTags)
{
    acorn::World world;

    auto e1 = world.create_entity();  // has Position only — should appear
    auto e2 = world.create_entity();  // has Position + FrozenTag — excluded
    auto e3 = world.create_entity();  // has Position + BossTag — excluded
    auto e4 = world.create_entity();  // has Position + both tags — excluded

    world.add<Position>(e1, 1.f, 1.f);
    world.add<Position>(e2, 2.f, 2.f);
    world.add<FrozenTag>(e2);
    world.add<Position>(e3, 3.f, 3.f);
    world.add<BossTag>(e3);
    world.add<Position>(e4, 4.f, 4.f);
    world.add<FrozenTag>(e4);
    world.add<BossTag>(e4);

    size_t count = 0;
    auto view = world.view_exclude<Position>(acorn::Exclude<FrozenTag, BossTag>{});
    view.each(
        [&](acorn::Entity e, Position& pos)
        {
            EXPECT_EQ(e, e1);
            EXPECT_FLOAT_EQ(pos.x, 1.f);
            count++;
        });

    EXPECT_EQ(count, 1);
}

TEST(ExcludeViewTest, ExcludeDoesNotAffectIncludedComponents)
{
    acorn::World world;

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add<Position>(e1, 10.f, 20.f);
    world.add<Velocity>(e1, 1.f, 2.f);

    world.add<Position>(e2, 30.f, 40.f);
    world.add<Velocity>(e2, 3.f, 4.f);
    world.add<FrozenTag>(e2);  // e2 excluded

    size_t count = 0;
    auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<FrozenTag>{});
    view.each(
        [&](acorn::Entity e, Position& pos, Velocity& vel)
        {
            EXPECT_EQ(e, e1);
            EXPECT_FLOAT_EQ(pos.x, 10.f);
            EXPECT_FLOAT_EQ(vel.dx, 1.f);
            count++;
        });

    EXPECT_EQ(count, 1);
}

TEST(ExcludeViewTest, AllEntitiesExcluded_ReturnsNothing)
{
    acorn::World world;

    for (int i = 0; i < 5; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, (float)i);
        world.add<PlayerTag>(e);
    }

    int count = 0;
    auto view = world.view_exclude<Position>(acorn::Exclude<PlayerTag>{});
    view.each([&](acorn::Entity, Position&) { count++; });

    EXPECT_EQ(count, 0);
}

TEST(ExcludeViewTest, ExcludeTagNotRegistered_AllIncludedReturned)
{
    acorn::World world;

    for (int i = 0; i < 4; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, 0.f);
    }

    size_t count = 0;
    auto view = world.view_exclude<Position>(acorn::Exclude<PlayerTag>{});
    view.each([&](acorn::Entity, Position&) { count++; });

    EXPECT_EQ(count, 4);
}

TEST(ExcludeViewTest, MutationDuringIteration_ComponentValuesCorrect)
{
    acorn::World world;

    for (int i = 0; i < 10; i++)
    {
        auto e = world.create_entity();
        world.add<Position>(e, (float)i, 0.f);
        world.add<Velocity>(e, 1.f, 0.f);
        if (i % 3 == 0)
            world.add<FrozenTag>(e);  // every 3rd is frozen
    }

    auto view = world.view_exclude<Position, Velocity>(acorn::Exclude<FrozenTag>{});
    view.each([](acorn::Entity, Position& pos, Velocity& vel) { pos.x += vel.dx; });

    auto all = world.view<Position, FrozenTag>();
    all.each([](acorn::Entity, Position& pos, FrozenTag&) { EXPECT_FLOAT_EQ(pos.y, 0.f); });
}