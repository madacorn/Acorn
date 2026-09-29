#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "entity.hpp"

namespace acorn
{
// Exact snapshot of the entity manager, for save/load. An index is alive when it is present in
// generations and not in free_list. free_list order matters: create() reuses from its back.
// For a free index, generations holds the generation its next handle will get.
struct EntityState
{
    std::vector<uint32_t> generations;
    std::vector<uint32_t> free_list;
};

// Generations are 31-bit and wrap after 2^31 reuses of a slot. While a slot is free its stored
// generation carries kFreeBit, which no issued handle has, so is_alive stays a single compare
// and rejects every handle to a free slot.
class EntityManager
{
public:
    static constexpr uint32_t kFreeBit = 1u << 31;
    static constexpr uint32_t kGenerationMask = kFreeBit - 1;

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

            generations_[idx] &= kGenerationMask;
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

        generations_[e.index] = ((generations_[e.index] + 1) & kGenerationMask) | kFreeBit;
        free_list_.push_back(e.index);
        return true;
    }

    bool is_alive(Entity e) const noexcept
    {
        return e.index < generations_.size() && e.generation == generations_[e.index] &&
               (e.generation & kFreeBit) == 0;
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
        EntityState state{generations_, free_list_};
        for (uint32_t& gen : state.generations) gen &= kGenerationMask;
        return state;
    }

    // Throws std::invalid_argument if a generation uses more than 31 bits, or a free-list index
    // is out of range or duplicated.
    void import_state(const EntityState& state)
    {
        for (uint32_t gen : state.generations)
        {
            if (gen & kFreeBit)
                throw std::invalid_argument(
                    "acorn::EntityManager: generation out of range in EntityState");
        }

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
        for (uint32_t idx : free_list_) generations_[idx] |= kFreeBit;
    }

private:
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> free_list_;
};

}  // namespace acorn
