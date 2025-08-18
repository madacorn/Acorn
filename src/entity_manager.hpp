#pragma once
#include <vector>

#include "entity.hpp"

namespace acorn
{
class EntityManager
{
public:
    explicit EntityManager(uint32_t max_hint = 0)
    {
        generations_.reserve(max_hint ? max_hint : 1024);
    }

    Entity create()
    {
        if (!free_list_.empty())
        {
            uint32_t idx = free_list_.back();
            free_list_.pop_back();

            return Entity{idx, generations_[idx]};
        }
        uint32_t idx = static_cast<uint32_t>(generations_.size());
        generations_.push_back(0);
        return {idx, 0};
    }

    bool destroy(Entity e)
    {
        if (!is_alive(e))
            return false;

        ++generations_[e.index];
        free_list_.push_back(e.index);
        return true;
    }

    bool is_alive(Entity e) const noexcept
    {
        return e.index < generations_.size() && e.generation == generations_[e.index];
    }

    uint32_t alive_count() const noexcept
    {
        return static_cast<uint32_t>(generations_.size() - free_list_.size());
    }

    uint32_t capacity() const noexcept
    {
        return static_cast<uint32_t>(generations_.size());
    }

private:
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> free_list_;
};

}  // namespace acorn
