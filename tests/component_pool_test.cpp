#include "component_pool.hpp"

#include <gtest/gtest.h>

TEST(ComponentPoolTest, HasOnEmptyIsFalse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    EXPECT_FALSE(pool.has(e));
}

TEST(ComponentPool, TryGetReturnsNullWhenAbsent)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);
    auto e = em.create();
    EXPECT_EQ(pool.try_get(e), nullptr);
}

TEST(ComponentPool, EmplaceThenHasAndTryGet)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 42);
    EXPECT_TRUE(pool.has(e));
    auto* p = pool.try_get(e);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(ComponentPool, DuplicateEmplaceOverwrites)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 1);
    pool.emplace(e, 7);
    EXPECT_EQ(*pool.try_get(e), 7);
}

TEST(ComponentPool, EmplaceLargeIndexGrowsSparse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    std::vector<acorn::Entity> v;
    for (int i = 0; i < 1000; ++i) v.push_back(em.create());
    auto e = v.back();
    pool.emplace(e, 9);
    EXPECT_TRUE(pool.has(e));
}
