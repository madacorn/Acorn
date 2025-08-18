#include "entity.hpp"

#include <gtest/gtest.h>

static_assert(acorn::Entity::null().is_null());
static_assert(acorn::Entity{}.is_null());

TEST(EntityTest, DefaultIsNull)
{
    acorn::Entity e{};
    EXPECT_TRUE(e.is_null());
}

TEST(EntityTest, NullEntity)
{
    acorn::Entity e = acorn::Entity::null();
    ASSERT_TRUE(e.is_null());
}

TEST(EntityTest, NonNullEntity)
{
    acorn::Entity e{1, 0};
    ASSERT_FALSE(e.is_null());
}

TEST(EntityTest, Equality)
{
    acorn::Entity a{1, 0};
    acorn::Entity b{1, 0};
    acorn::Entity c{2, 0};

    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a == c);
}