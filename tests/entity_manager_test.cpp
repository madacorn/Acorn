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

TEST(EntityManagerTest, HandleWithCurrentGenerationOfFreeSlotIsNotAlive)
{
    acorn::EntityManager em;
    acorn::Entity e = em.create();
    ASSERT_TRUE(em.destroy(e));

    // Handle the slot will hand out next: must not be alive until create() issues it.
    acorn::Entity next{e.index, e.generation + 1};
    EXPECT_FALSE(em.is_alive(next));
    EXPECT_FALSE(em.destroy(next));

    acorn::Entity reused = em.create();
    EXPECT_EQ(reused, next);
    EXPECT_TRUE(em.is_alive(reused));
}

TEST(EntityManagerTest, NoHandleToFreeSlotIsAlive)
{
    acorn::EntityManager em;
    acorn::Entity e = em.create();
    ASSERT_TRUE(em.destroy(e));

    for (uint32_t gen : {0u, 1u, 2u, acorn::EntityManager::kFreeBit,
                         acorn::EntityManager::kFreeBit | 1u, UINT32_MAX})
    {
        EXPECT_FALSE(em.is_alive(acorn::Entity{e.index, gen})) << gen;
    }
}

TEST(EntityManagerTest, GenerationWrapsWithin31Bits)
{
    acorn::EntityManager em;
    acorn::EntityState state{{acorn::EntityManager::kGenerationMask}, {}};
    em.import_state(state);

    acorn::Entity e{0, acorn::EntityManager::kGenerationMask};
    ASSERT_TRUE(em.is_alive(e));
    ASSERT_TRUE(em.destroy(e));

    acorn::Entity reused = em.create();
    EXPECT_EQ(reused.generation, 0u);
    EXPECT_TRUE(em.is_alive(reused));
    EXPECT_FALSE(em.is_alive(e));
}
