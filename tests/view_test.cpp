#include "view.hpp"

#include <gtest/gtest.h>

#include "world.hpp"

TEST(ViewTest, EmptyViewReturnsNothing)
{
    acorn::World world;
    world.create_entity();  // Entity with nothing

    auto view = world.view<int, float>();
    int count = 0;
    for (auto e : view)
    {
        count++;
    }

    EXPECT_EQ(count, 0);
}

TEST(ViewTest, FiltersEntitiesWithMultipleComponents)
{
    acorn::World world;
    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add<int>(e1, 10);
    world.add<float>(e1, 1.0f);
    world.add<int>(e2, 20);

    auto view = world.view<int, float>();

    size_t count = 0;

    for (auto [e, i, f] : view)
    {
        EXPECT_EQ(e1, e);
        EXPECT_EQ(i, 10);
        EXPECT_EQ(f, 1.0f);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST(ViewTest, MultipleComponentPartialMatch)
{
    acorn::World world;
    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add<int>(e1, 1);
    world.add<float>(e1, 1.0f);
    world.add<char>(e1, 'a');

    world.add<int>(e2, 2);
    world.add<float>(e2, 2.0f);

    auto view = world.view<int, float, char>();

    size_t count = 0;

    for (auto [e, i, f, c] : view)
    {
        EXPECT_EQ(e1, e);
        EXPECT_EQ(c, 'a');
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST(ViewTest, EachIterationProvidesCorrectReferences)
{
    acorn::World world;
    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add<int>(e1, 10);
    world.add<float>(e1, 1.0f);
    
    // e2 only has int, should be skipped by view<int, float>
    world.add<int>(e2, 20);

    auto view = world.view<int, float>();

    size_t count = 0;
    view.each([&](acorn::Entity e, int& i, float& f) {
        EXPECT_EQ(e, e1);
        EXPECT_EQ(i, 10);
        EXPECT_EQ(f, 1.0f);
        
        // Verify we can modify the components directly
        i = 50;
        count++;
    });

    EXPECT_EQ(count, 1);
    EXPECT_EQ(world.get<int>(e1), 50);
}