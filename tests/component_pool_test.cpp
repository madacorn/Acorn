#include "component_pool.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "entity_manager.hpp"

TEST(ComponentPoolTest, HasOnEmptyIsFalse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    EXPECT_FALSE(pool.has(e));
}

TEST(ComponentPoolTest, TryGetReturnsNullWhenAbsent)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);
    auto e = em.create();
    EXPECT_EQ(pool.try_get(e), nullptr);
}

TEST(ComponentPoolTest, EmplaceThenHasAndTryGet)
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

TEST(ComponentPoolTest, DuplicateEmplaceOverwrites)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 1);
    pool.emplace(e, 7);
    EXPECT_EQ(*pool.try_get(e), 7);
}

TEST(ComponentPoolTest, EmplaceLargeIndexGrowsSparse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    std::vector<acorn::Entity> v;
    for (int i = 0; i < 1000; ++i) v.push_back(em.create());
    auto e = v.back();
    pool.emplace(e, 9);
    EXPECT_TRUE(pool.has(e));
}

TEST(ComponentPoolTest, RemoveExistingClearsHasAndShrinks)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 5);
    ASSERT_TRUE(pool.has(e));
    const auto before = pool.size();

    EXPECT_TRUE(pool.remove(e));
    EXPECT_FALSE(pool.has(e));
    EXPECT_EQ(pool.size(), before - 1);
}

TEST(ComponentPoolTest, RemoveMiddleSwapRemoveUpdatesSparse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e0 = em.create();
    auto e1 = em.create();
    auto e2 = em.create();

    pool.emplace(e0, 10);
    pool.emplace(e1, 20);
    pool.emplace(e2, 30);

    EXPECT_TRUE(pool.remove(e1));
    EXPECT_FALSE(pool.has(e1));
    EXPECT_TRUE(pool.has(e0));
    EXPECT_TRUE(pool.has(e2));

    EXPECT_EQ(*pool.try_get(e0), 10);
    EXPECT_EQ(*pool.try_get(e2), 30);
}

TEST(ComponentPoolTest, RemoveLastFastPath)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto a = em.create();
    auto b = em.create();

    pool.emplace(a, 1);
    pool.emplace(b, 2);  // b is last

    EXPECT_TRUE(pool.remove(b));  // no swap
    EXPECT_TRUE(pool.has(a));
    EXPECT_FALSE(pool.has(b));
    EXPECT_EQ(*pool.try_get(a), 1);
}

TEST(ComponentPoolTest, RemoveAbsentReturnsFalse)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    EXPECT_FALSE(pool.remove(e));  // never added
    pool.emplace(e, 1);
    EXPECT_TRUE(pool.remove(e));
    EXPECT_FALSE(pool.remove(e));  // second time = false
}

TEST(ComponentPoolTest, IterateVisitsAll)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto a = em.create();
    auto b = em.create();
    pool.emplace(a, 3);
    pool.emplace(b, 4);

    size_t count = 0;
    int sum = 0;
    for (int& v : pool)
    {
        ++count;
        sum += v;
    }
    EXPECT_EQ(count, pool.size());
    EXPECT_EQ(sum, 7);
}

TEST(ComponentPoolTest, HasRespectsGenerationAfterDestroy)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 5);
    ASSERT_TRUE(pool.has(e));

    EXPECT_TRUE(em.destroy(e));
    EXPECT_FALSE(pool.has(e));
    EXPECT_EQ(pool.try_get(e), nullptr);
}

TEST(ComponentPoolTest, GetReturnsReference)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);
    auto e = em.create();
    pool.emplace(e, 5);
    int& ref = pool.get(e);
    ref = 9;
    EXPECT_EQ(*pool.try_get(e), 9);  // proves aliasing, not a copy
}

TEST(ComponentPoolTest, ConstTryGetAndGet)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);
    auto e = em.create();
    pool.emplace(e, 11);

    const auto& cpool = pool;
    const int* p = cpool.try_get(e);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 11);
    EXPECT_EQ(cpool.get(e), 11);
}

TEST(ComponentPoolTest, GetAbsentThrows)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);
    auto e = em.create();
    EXPECT_THROW((void)pool.get(e), std::logic_error);
}

TEST(ComponentPoolTest, ReAddAfterRemoveWorks)
{
    acorn::EntityManager em;
    acorn::ComponentPool<int> pool(em);

    auto e = em.create();
    pool.emplace(e, 5);
    EXPECT_TRUE(pool.remove(e));
    EXPECT_FALSE(pool.has(e));

    pool.emplace(e, 7);
    EXPECT_TRUE(pool.has(e));
    EXPECT_EQ(*pool.try_get(e), 7);
}
