#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include "world.hpp"

using namespace acorn;

namespace
{
struct Name
{
    int id{};
};

struct Pos
{
    float x{}, y{};
};

struct Target
{
    Entity target = Entity::null();
};

struct Snapshot
{
    EntityState entities;
    std::vector<std::pair<Entity, Name>> names;
    std::vector<std::pair<Entity, Pos>> positions;
    std::vector<std::pair<Entity, Target>> targets;
};

Snapshot save(const World& w)
{
    Snapshot s;
    s.entities = w.export_entities();
    w.each_in_order<Name>([&](Entity e, const Name& c) { s.names.emplace_back(e, c); });
    w.each_in_order<Pos>([&](Entity e, const Pos& c) { s.positions.emplace_back(e, c); });
    w.each_in_order<Target>([&](Entity e, const Target& c) { s.targets.emplace_back(e, c); });
    return s;
}

void load(World& w, const Snapshot& s)
{
    w.import_entities(s.entities);
    for (const auto& [e, c] : s.names) w.restore_component<Name>(e, c);
    for (const auto& [e, c] : s.positions) w.restore_component<Pos>(e, c);
    for (const auto& [e, c] : s.targets) w.restore_component<Target>(e, c);
}

// Builds a world with dead entities, reused slots, and pools whose dense order differs from
// creation order (swap-and-pop removals).
World& build(World& w, std::vector<Entity>& alive)
{
    std::vector<Entity> all;
    for (int i = 0; i < 12; ++i)
    {
        Entity e = w.create_entity();
        w.add<Name>(e, Name{i});
        if (i % 2 == 0)
            w.add<Pos>(e, Pos{float(i), float(-i)});
        all.push_back(e);
    }

    w.destroy_entity(all[1]);
    w.destroy_entity(all[4]);
    w.destroy_entity(all[7]);
    w.destroy_entity(all[10]);

    // Reuse slot of all[10] (back of the free list) with a new generation.
    Entity reused = w.create_entity();
    w.add<Name>(reused, Name{100});
    w.add<Pos>(reused, Pos{1.f, 2.f});

    w.remove<Pos>(all[0]);
    w.add<Pos>(all[0], Pos{9.f, 9.f});

    for (Entity e : all)
        if (w.is_alive(e))
            alive.push_back(e);
    alive.push_back(reused);

    // Handles stored in components, including one to the reused slot.
    w.add<Target>(all[3], Target{reused});
    w.add<Target>(all[2], Target{all[11]});
    w.add<Target>(all[11], Target{all[3]});
    return w;
}

template <typename... Ts>
std::vector<Entity> view_order(const World& w)
{
    std::vector<Entity> out;
    w.view<Ts...>().each([&](Entity e, const Ts&...) { out.push_back(e); });
    return out;
}

std::vector<Entity> exclude_order(const World& w)
{
    std::vector<Entity> out;
    w.view_exclude<Name>(Exclude<Pos>{}).each([&](Entity e, const Name&) { out.push_back(e); });
    return out;
}
}  // namespace

TEST(SerializationTest, RoundTripEntitiesIntoFreshWorld)
{
    World original;
    std::vector<Entity> alive;
    build(original, alive);

    const Snapshot snap = save(original);
    World loaded;
    load(loaded, snap);

    const auto state = loaded.export_entities();
    EXPECT_EQ(state.generations, snap.entities.generations);
    EXPECT_EQ(state.free_list, snap.entities.free_list);

    for (Entity e : alive)
    {
        EXPECT_TRUE(loaded.is_alive(e));
        EXPECT_EQ(loaded.get<Name>(e).id, original.get<Name>(e).id);
    }

    // Stale handle to the reused slot's previous occupant stays dead.
    Entity stale{alive.back().index, alive.back().generation - 1};
    EXPECT_FALSE(original.is_alive(stale));
    EXPECT_FALSE(loaded.is_alive(stale));
    EXPECT_FALSE(loaded.is_alive(Entity{1, 0}));
}

TEST(SerializationTest, StoredHandlesStillPointToSameEntity)
{
    World original;
    std::vector<Entity> alive;
    build(original, alive);

    World loaded;
    load(loaded, save(original));

    size_t checked = 0;
    loaded.view<Target>().each(
        [&](Entity e, const Target& t)
        {
            ASSERT_TRUE(loaded.is_alive(t.target));
            EXPECT_EQ(loaded.get<Name>(t.target).id,
                      original.get<Name>(original.get<Target>(e).target).id);
            ++checked;
        });
    EXPECT_EQ(checked, 3u);
}

TEST(SerializationTest, ViewOrderIdenticalAfterClearAndReload)
{
    World w;
    std::vector<Entity> alive;
    build(w, alive);

    const auto names = view_order<Name>(w);
    const auto positions = view_order<Pos>(w);
    const auto name_pos = view_order<Name, Pos>(w);
    const auto pos_name = view_order<Pos, Name>(w);
    const auto name_target = view_order<Name, Target>(w);
    const auto excluded = exclude_order(w);

    const Snapshot snap = save(w);
    w.clear();
    load(w, snap);

    EXPECT_EQ(view_order<Name>(w), names);
    EXPECT_EQ(view_order<Pos>(w), positions);
    EXPECT_EQ((view_order<Name, Pos>(w)), name_pos);
    EXPECT_EQ((view_order<Pos, Name>(w)), pos_name);
    EXPECT_EQ((view_order<Name, Target>(w)), name_target);
    EXPECT_EQ(exclude_order(w), excluded);

    // Dense order is not creation order, so this test is meaningful.
    std::vector<Entity> sorted = positions;
    std::sort(sorted.begin(), sorted.end(), [](Entity a, Entity b) { return a.index < b.index; });
    EXPECT_NE(sorted, positions);
}

TEST(SerializationTest, NextCreatedEntityMatches)
{
    World original;
    std::vector<Entity> alive;
    build(original, alive);

    World loaded;
    load(loaded, save(original));

    // Drains the free list and then allocates fresh slots.
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(loaded.create_entity(), original.create_entity());
    }
}

TEST(SerializationTest, ImportOnNonEmptyWorldIsRejected)
{
    World source;
    source.create_entity();
    const auto state = source.export_entities();

    World with_entity;
    with_entity.create_entity();
    EXPECT_THROW(with_entity.import_entities(state), std::logic_error);

    World with_dead_slot;
    with_dead_slot.destroy_entity(with_dead_slot.create_entity());
    EXPECT_THROW(with_dead_slot.import_entities(state), std::logic_error);

    World cleared;
    cleared.add<Name>(cleared.create_entity(), Name{1});
    cleared.clear();
    EXPECT_NO_THROW(cleared.import_entities(state));
}

TEST(SerializationTest, ImportRejectsMalformedState)
{
    World w;
    EXPECT_THROW(w.import_entities(EntityState{{0, 0}, {2}}), std::invalid_argument);
    EXPECT_THROW(w.import_entities(EntityState{{1, 1}, {0, 0}}), std::invalid_argument);
}

TEST(SerializationTest, ImportRejectsGenerationUsingFreeBit)
{
    World w;
    EXPECT_THROW(w.import_entities(EntityState{{EntityManager::kFreeBit}, {}}),
                 std::invalid_argument);
}

TEST(SerializationTest, FreeSlotsStayDeadAfterImport)
{
    World w;
    w.import_entities(EntityState{{0, 1}, {1}});

    EXPECT_TRUE(w.is_alive(Entity{0, 0}));
    EXPECT_FALSE(w.is_alive(Entity{1, 0}));
    EXPECT_FALSE(w.is_alive(Entity{1, 1}));

    const auto state = w.export_entities();
    EXPECT_EQ(state.generations, (std::vector<uint32_t>{0, 1}));
    EXPECT_EQ(state.free_list, (std::vector<uint32_t>{1}));

    EXPECT_EQ(w.create_entity(), (Entity{1, 1}));
}

TEST(SerializationTest, RestoreComponentRejectsDeadOrDuplicate)
{
    World w;
    w.import_entities(EntityState{{0, 1}, {1}});

    EXPECT_THROW(w.restore_component<Name>(Entity{1, 0}, Name{}), std::logic_error);
    EXPECT_THROW(w.restore_component<Name>(Entity{1, 1}, Name{}), std::logic_error);
    w.restore_component<Name>(Entity{0, 0}, Name{1});
    EXPECT_THROW(w.restore_component<Name>(Entity{0, 0}, Name{2}), std::logic_error);
    EXPECT_EQ(w.get<Name>(Entity{0, 0}).id, 1);
}

TEST(SerializationTest, EachInOrderOnUnregisteredTypeVisitsNothing)
{
    const World w;
    int visits = 0;
    w.each_in_order<Name>([&](Entity, const Name&) { ++visits; });
    EXPECT_EQ(visits, 0);
}
