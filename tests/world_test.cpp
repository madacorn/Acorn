#include "world.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "component_pool.hpp"
#include "entity_manager.hpp"

using namespace acorn;

struct CompA
{
    int x{};
};

struct CompB
{
    float y{};
};

TEST(WorldTest, CreateAndDestroyEntity)
{
    World w;
    Entity e = w.create_entity();

    EXPECT_FALSE(w.has<int>(e));
    w.add<int>(e, 7);
    EXPECT_TRUE(w.has<int>(e));
    EXPECT_TRUE(w.destroy_entity(e));
    EXPECT_FALSE(w.has<int>(e));
    EXPECT_EQ(w.try_get<int>(e), nullptr);
}

TEST(WorldTest, PoolLazyCreateAndStable)
{
    World w;
    auto& p1 = w.pool<int>();
    auto& p2 = w.pool<int>();
    EXPECT_EQ(&p1, &p2);
}

TEST(WorldTest, AddGetRemoveRoundtrip_Primitives)
{
    World w;
    Entity e = w.create_entity();

    w.add<int>(e, 42);
    EXPECT_TRUE(w.has<int>(e));
    EXPECT_NE(w.try_get<int>(e), nullptr);
    EXPECT_EQ(w.get<int>(e), 42);

    EXPECT_TRUE(w.remove<int>(e));
    EXPECT_FALSE(w.has<int>(e));
    EXPECT_EQ(w.try_get<int>(e), nullptr);
}

TEST(WorldTest, AddGetRemoveRoundtrip_Aggregates)
{
    World w;
    Entity e = w.create_entity();

    w.add<CompA>(e, CompA{5});
    w.add<CompB>(e, CompB{1.5f});

    EXPECT_TRUE(w.has<CompA>(e));
    EXPECT_TRUE(w.has<CompB>(e));
    EXPECT_EQ(w.get<CompA>(e).x, 5);
    EXPECT_FLOAT_EQ(w.get<CompB>(e).y, 1.5f);

    EXPECT_TRUE(w.remove<CompA>(e));
    EXPECT_FALSE(w.has<CompA>(e));
    EXPECT_TRUE(w.has<CompB>(e));
}

TEST(WorldTest, GetAbsentThrows)
{
    World w;
    Entity e = w.create_entity();
    EXPECT_THROW((void)w.get<int>(e), std::logic_error);

    EXPECT_THROW((void)w.get<CompA>(e), std::logic_error);
}

TEST(WorldTest, TryGetReturnsNullWhenPoolMissingOrAbsent)
{
    World w;
    Entity e = w.create_entity();

    EXPECT_EQ(w.try_get<int>(e), nullptr);

    w.add<int>(e, 9);
    EXPECT_NE(w.try_get<int>(e), nullptr);

    w.remove<int>(e);
    EXPECT_EQ(w.try_get<int>(e), nullptr);
}

TEST(WorldTest, ReAddAfterRemoveWorks)
{
    World w;
    Entity e = w.create_entity();

    w.add<int>(e, 3);
    EXPECT_TRUE(w.remove<int>(e));
    EXPECT_FALSE(w.has<int>(e));

    w.add<int>(e, 8);
    EXPECT_TRUE(w.has<int>(e));
    EXPECT_EQ(w.get<int>(e), 8);
}

TEST(WorldTest, MultipleEntitiesIndependent)
{
    World w;
    Entity a = w.create_entity();
    Entity b = w.create_entity();

    w.add<int>(a, 1);
    w.add<int>(b, 2);

    EXPECT_EQ(w.get<int>(a), 1);
    EXPECT_EQ(w.get<int>(b), 2);

    EXPECT_TRUE(w.remove<int>(a));
    EXPECT_FALSE(w.has<int>(a));
    EXPECT_TRUE(w.has<int>(b));
    EXPECT_EQ(w.get<int>(b), 2);
}

TEST(WorldTest, RemoveReturnsFalseWhenPoolMissingOrNoComponent)
{
    World w;
    Entity e = w.create_entity();

    EXPECT_FALSE(w.remove<int>(e));

    w.pool<int>();
    EXPECT_FALSE(w.remove<int>(e));
}

TEST(WorldTest, ConstTryGetAndGetThroughWorld)
{
    World w;
    Entity e = w.create_entity();
    w.add<int>(e, 11);

    const World& cw = w;
    const int* p = cw.try_get<int>(e);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 11);
    EXPECT_EQ(cw.get<int>(e), 11);
}
