#include <gtest/gtest.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "world.hpp"

using namespace acorn;

namespace
{
struct Pos
{
    int x{};
};

struct Vel
{
    int dx{};
};

struct Frozen
{
};

static_assert(!std::is_copy_constructible_v<World>);
static_assert(!std::is_copy_assignable_v<World>);
static_assert(std::is_nothrow_move_constructible_v<World>);
static_assert(std::is_nothrow_move_assignable_v<World>);

struct Fixture
{
    World world;
    Entity moving;  // Pos + Vel
    Entity frozen;  // Pos + Vel + Frozen
    Entity still;   // Pos only
    Entity dead;
    std::vector<Entity> destroyed;

    Fixture()
    {
        dead = world.create_entity();
        moving = world.create_entity();
        frozen = world.create_entity();
        still = world.create_entity();
        world.add<Pos>(dead, Pos{-1});
        world.destroy_entity(dead);

        world.add<Pos>(moving, Pos{1});
        world.add<Vel>(moving, Vel{10});
        world.add<Pos>(frozen, Pos{2});
        world.add<Vel>(frozen, Vel{20});
        world.add<Frozen>(frozen);
        world.add<Pos>(still, Pos{3});

        world.on_destroy([this](World&, Entity e) { destroyed.push_back(e); });
    }
};

void expect_world_intact(World& w, const Fixture& f)
{
    EXPECT_TRUE(w.is_alive(f.moving));
    EXPECT_TRUE(w.is_alive(f.frozen));
    EXPECT_TRUE(w.is_alive(f.still));
    EXPECT_FALSE(w.is_alive(f.dead));

    EXPECT_TRUE(w.has<Pos>(f.moving));
    EXPECT_TRUE(w.has<Vel>(f.moving));
    EXPECT_TRUE(w.has<Frozen>(f.frozen));
    EXPECT_FALSE(w.has<Vel>(f.still));
    EXPECT_FALSE(w.has<Pos>(f.dead));
    EXPECT_EQ(w.get<Pos>(f.still).x, 3);
    EXPECT_EQ(w.get<Vel>(f.frozen).dx, 20);

    std::vector<Entity> with_pos;
    w.view<Pos>().each([&](Entity e, Pos&) { with_pos.push_back(e); });
    EXPECT_EQ(with_pos, (std::vector<Entity>{f.moving, f.frozen, f.still}));

    std::vector<Entity> pos_vel;
    for (auto [e, p, v] : w.view<Pos, Vel>())
    {
        (void)p;
        (void)v;
        pos_vel.push_back(e);
    }
    EXPECT_EQ(pos_vel, (std::vector<Entity>{f.moving, f.frozen}));

    std::vector<Entity> not_frozen;
    w.view_exclude<Pos, Vel>(Exclude<Frozen>{})
        .each([&](Entity e, Pos&, Vel&) { not_frozen.push_back(e); });
    EXPECT_EQ(not_frozen, (std::vector<Entity>{f.moving}));

    const World& cw = w;
    size_t const_count = 0;
    cw.view<Pos, Vel>().each([&](Entity, const Pos&, const Vel&) { ++const_count; });
    EXPECT_EQ(const_count, 2u);

    // Adding still works (emplace asserts liveness against the pool's manager).
    w.add<Vel>(f.still, Vel{30});
    EXPECT_EQ(w.get<Vel>(f.still).dx, 30);
}
}  // namespace

TEST(WorldMoveTest, MoveConstructRebindsPools)
{
    Fixture f;
    World moved = std::move(f.world);

    expect_world_intact(moved, f);
}

TEST(WorldMoveTest, MoveAssignRebindsPools)
{
    Fixture f;
    World target;
    Entity old = target.create_entity();
    target.add<Pos>(old, Pos{99});
    target.add<int>(old, 5);

    target = std::move(f.world);

    expect_world_intact(target, f);
    // The target's previous contents are gone.
    EXPECT_EQ(target.pool<int>().size(), 0u);
}

TEST(WorldMoveTest, CreateEntityContinuesWithSameHandles)
{
    Fixture reference;
    Fixture f;
    World moved = std::move(f.world);

    // Reuses the dead slot first, then allocates fresh indices.
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(moved.create_entity(), reference.world.create_entity());
    }
}

TEST(WorldMoveTest, DestroyListenersAndDeferredCommandsMove)
{
    Fixture f;
    f.world.defer_destroy(f.still);

    World moved = std::move(f.world);

    ASSERT_TRUE(moved.destroy_entity(f.moving));
    moved.flush();

    EXPECT_EQ(f.destroyed, (std::vector<Entity>{f.moving, f.still}));
    EXPECT_FALSE(moved.is_alive(f.moving));
    EXPECT_FALSE(moved.is_alive(f.still));
    EXPECT_FALSE(moved.has<Pos>(f.moving));
    EXPECT_EQ(moved.pool<Pos>().size(), 1u);
}

TEST(WorldMoveTest, MovedFromWorldIsEmptyAndIndependent)
{
    Fixture f;
    World moved = std::move(f.world);

    // Moved-from world is empty and usable.
    EXPECT_FALSE(f.world.is_alive(f.moving));
    EXPECT_EQ(f.world.pool<Pos>().size(), 0u);
    f.world.flush();  // no deferred commands left behind

    Entity fresh = f.world.create_entity();
    EXPECT_EQ(fresh, (Entity{0, 0}));
    f.world.add<Pos>(fresh, Pos{7});
    EXPECT_TRUE(f.world.destroy_entity(fresh));
    EXPECT_TRUE(f.destroyed.empty());  // listeners moved away

    f.world.clear();
    expect_world_intact(moved, f);
}

TEST(WorldMoveTest, DestroyingMovedFromWorldDoesNotAffectDestination)
{
    Fixture f;
    World moved;
    {
        World source = std::move(f.world);
        moved = std::move(source);
    }  // source destroyed here

    expect_world_intact(moved, f);
}

TEST(WorldMoveTest, SelfMoveAssignIsNoOp)
{
    Fixture f;
    World& alias = f.world;
    f.world = std::move(alias);

    expect_world_intact(f.world, f);
}
