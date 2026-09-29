#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "entity.hpp"

namespace acorn
{
// Exact snapshot of the entity manager, for save/load. An index is alive when it is present in
// generations and not in free_list. free_list order matters: create() reuses from its back.
struct EntityState
{
    std::vector<uint32_t> generations;
    std::vector<uint32_t> free_list;
};

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

    void reset() noexcept
    {
        generations_.clear();
        free_list_.clear();
    }

    [[nodiscard]] EntityState export_state() const
    {
        return EntityState{generations_, free_list_};
    }

    // Throws std::invalid_argument if a free-list index is out of range or duplicated.
    void import_state(const EntityState& state)
    {
        std::vector<bool> seen(state.generations.size(), false);
        for (uint32_t idx : state.free_list)
        {
            if (idx >= state.generations.size() || seen[idx])
                throw std::invalid_argument(
                    "acorn::EntityManager: invalid free list in EntityState");
            seen[idx] = true;
        }

        generations_ = state.generations;
        free_list_ = state.free_list;
    }

private:
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> free_list_;
};

}  // namespace acorn
