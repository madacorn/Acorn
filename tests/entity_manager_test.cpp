#include "entity_manager.hpp"

#include <gtest/gtest.h>

TEST(EntityManagerTest, CreateIsAlive)
{
    acorn::EntityManager em;
    acorn::Entity e = em.create();

    ASSERT_TRUE(em.is_alive(e));
    EXPECT_GT(em.capacity(), 0);
    EXPECT_EQ(em.alive_count(), 1);
}

TEST(EntityManagerTest, DestroyInvalidatesHandle)
{
    acorn::EntityManager em;
    acorn::Entity e = em.create();
    EXPECT_TRUE(em.destroy(e));
    EXPECT_FALSE(em.is_alive(e));
    EXPECT_EQ(em.alive_count(), 0);
}

TEST(EntityManagerTest, ReuseIndexWithGenerationBump)
{
    acorn::EntityManager em;
    acorn::Entity e1 = em.create();
    uint32_t idx = e1.index;

    EXPECT_TRUE(em.destroy(e1));

    acorn::Entity e2 = em.create();
    EXPECT_EQ(e2.index, idx);
    EXPECT_NE(e2.generation, e1.generation);
    EXPECT_FALSE(em.is_alive(e1));
    EXPECT_TRUE(em.is_alive(e2));
}

TEST(EntityManagerTest, DestroyTwiceReturnsFalse)
{
    acorn::EntityManager em;
    acorn::Entity e = em.create();
    EXPECT_TRUE(em.destroy(e));
    EXPECT_FALSE(em.destroy(e));
    EXPECT_FALSE(em.is_alive(e));
    EXPECT_EQ(em.alive_count(), 0);
}

TEST(EntityManagerTest, NullIsNotAlive)
{
    acorn::EntityManager em;

    EXPECT_FALSE(em.is_alive(acorn::Entity::null()));
}

TEST(EntityManagerTest, CapacityDoesNotGrowWhenReusing)
{
    acorn::EntityManager em;
    acorn::Entity e1 = em.create();
    acorn::Entity e2 = em.create();

    uint32_t cap = em.capacity();
    em.destroy(e1);
    acorn::Entity e3 = em.create();
    EXPECT_EQ(em.capacity(), cap);
    EXPECT_FALSE(em.is_alive(e1));
    EXPECT_TRUE(em.is_alive(e2));
    EXPECT_TRUE(em.is_alive(e3));
}
