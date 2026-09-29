#include <gtest/gtest.h>

#include <vector>

#include "world.hpp"

using namespace acorn;

namespace
{
struct Health
{
    int hp{};
};

struct Reservation
{
    Entity holder = Entity::null();
};
}  // namespace

TEST(DestroyEventsTest, ListenerFiresOnceWithDestroyedEntity)
{
    World w;
    Entity a = w.create_entity();
    Entity b = w.create_entity();

    std::vector<Entity> seen;
    w.on_destroy([&](World&, Entity e) { seen.push_back(e); });

    ASSERT_TRUE(w.destroy_entity(b));
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], b);

    // Destroying an already dead entity does not fire again.
    EXPECT_FALSE(w.destroy_entity(b));
    EXPECT_EQ(seen.size(), 1u);
    EXPECT_TRUE(w.is_alive(a));
}

TEST(DestroyEventsTest, ComponentsReadableInsideListener)
{
    World w;
    Entity e = w.create_entity();
    w.add<Health>(e, Health{42});

    int read_hp = -1;
    bool alive_inside = false;
    w.on_destroy(
        [&](World& world, Entity dying)
        {
            alive_inside = world.is_alive(dying);
            if (const auto* h = world.try_get<Health>(dying))
                read_hp = h->hp;
        });

    w.destroy_entity(e);

    EXPECT_TRUE(alive_inside);
    EXPECT_EQ(read_hp, 42);
    EXPECT_FALSE(w.is_alive(e));
    EXPECT_FALSE(w.has<Health>(e));
    EXPECT_EQ(w.pool<Health>().size(), 0u);
}

TEST(DestroyEventsTest, ListenersRunInRegistrationOrder)
{
    World w;
    Entity e = w.create_entity();

    std::vector<int> order;
    w.on_destroy([&](World&, Entity) { order.push_back(1); });
    w.on_destroy([&](World&, Entity) { order.push_back(2); });
    w.on_destroy([&](World&, Entity) { order.push_back(3); });

    w.destroy_entity(e);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(DestroyEventsTest, DeferDestroyFiresOnFlush)
{
    World w;
    Entity e = w.create_entity();
    w.add<Health>(e, Health{7});

    std::vector<Entity> seen;
    int read_hp = -1;
    w.on_destroy(
        [&](World& world, Entity dying)
        {
            seen.push_back(dying);
            read_hp = world.get<Health>(dying).hp;
        });

    w.defer_destroy(e);
    EXPECT_TRUE(seen.empty());

    w.flush();

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0], e);
    EXPECT_EQ(read_hp, 7);
    EXPECT_FALSE(w.is_alive(e));
}

TEST(DestroyEventsTest, ListenerClearsHandleStoredInAnotherComponent)
{
    World w;
    Entity holder = w.create_entity();
    Entity seat = w.create_entity();
    w.add<Reservation>(seat, Reservation{holder});

    w.on_destroy(
        [](World& world, Entity dying)
        {
            for (auto [e, r] : world.view<Reservation>())
            {
                if (r.holder == dying)
                    r.holder = Entity::null();
            }
        });

    w.destroy_entity(holder);

    EXPECT_TRUE(w.get<Reservation>(seat).holder.is_null());
}

TEST(DestroyEventsTest, ClearDoesNotFire)
{
    World w;
    w.create_entity();
    w.create_entity();

    int fired = 0;
    w.on_destroy([&](World&, Entity) { ++fired; });

    w.clear();

    EXPECT_EQ(fired, 0);
}

TEST(DestroyEventsTest, DestroyInsideListenerIsQueued)
{
    World w;
    Entity parent = w.create_entity();
    Entity child = w.create_entity();
    Entity grandchild = w.create_entity();
    w.add<Reservation>(child, Reservation{parent});
    w.add<Reservation>(grandchild, Reservation{child});

    std::vector<Entity> seen;
    w.on_destroy(
        [&](World& world, Entity dying)
        {
            seen.push_back(dying);
            // Cascade: destroy everything that references the dying entity.
            std::vector<Entity> dependents;
            world.view<Reservation>().each(
                [&](Entity e, Reservation& r)
                {
                    if (r.holder == dying)
                        dependents.push_back(e);
                });
            for (Entity d : dependents)
            {
                EXPECT_TRUE(world.destroy_entity(d));
                // Queued: still alive until the current destruction finishes.
                EXPECT_TRUE(world.is_alive(d));
            }
        });

    w.destroy_entity(parent);

    EXPECT_EQ(seen, (std::vector<Entity>{parent, child, grandchild}));
    EXPECT_FALSE(w.is_alive(parent));
    EXPECT_FALSE(w.is_alive(child));
    EXPECT_FALSE(w.is_alive(grandchild));
    EXPECT_EQ(w.pool<Reservation>().size(), 0u);
}

TEST(DestroyEventsTest, ListenerCanDeferDuringFlush)
{
    World w;
    Entity a = w.create_entity();
    Entity b = w.create_entity();
    w.add<Health>(b, Health{1});

    w.on_destroy(
        [&](World& world, Entity dying)
        {
            if (dying == a)
            {
                world.defer_remove<Health>(b);
                world.defer_destroy(b);
            }
        });

    w.defer_destroy(a);
    w.flush();

    EXPECT_FALSE(w.is_alive(a));
    EXPECT_FALSE(w.is_alive(b));
    EXPECT_EQ(w.pool<Health>().size(), 0u);
}
