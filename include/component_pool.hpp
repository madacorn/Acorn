#pragma once
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "acorn_assert.hpp"
#include "entity.hpp"

namespace acorn
{
// Sparse set of T keyed by Entity. The pool does not track entity liveness: a handle matches only
// if its index and generation equal the stored entity, so the owner (World) must remove an
// entity's components when it destroys it.
template <typename T>
class ComponentPool
{
public:
    explicit ComponentPool(size_t reserve_hint = 0) noexcept
    {
        if (reserve_hint)
        {
            dense_entities_.reserve(reserve_hint);
            dense_data_.reserve(reserve_hint);
            sparse_.reserve(reserve_hint);
        }
    }

    bool has(Entity e) const noexcept
    {
        if (e.index >= sparse_.size())
            return false;

        uint32_t pos = sparse_[e.index];
        if (pos == kAbsent)
            return false;

        return dense_entities_[pos] == e;
    }

    T* try_get(Entity e) noexcept
    {
        if (!has(e))
            return nullptr;

        return &dense_data_[sparse_[e.index]];
    }

    const T* try_get(Entity e) const noexcept
    {
        if (!has(e))
            return nullptr;
        return &dense_data_[sparse_[e.index]];
    }

    [[nodiscard]] T& get(Entity e)
    {
        if (auto* p = try_get(e))
            return *p;
        throw std::out_of_range(
            "acorn::ComponentPool: entity does not have the requested component");
    }

    [[nodiscard]] const T& get(Entity e) const
    {
        if (auto* p = try_get(e))
            return *p;
        throw std::out_of_range(
            "acorn::ComponentPool: entity does not have the requested component");
    }

    [[nodiscard]] const std::vector<Entity>& entities() const noexcept
    {
        return dense_entities_;
    }

    template <typename... Args>
    T& emplace(Entity e, Args&&... args)
    {
        grow_sparse_to_fit(e.index);

        // A different generation in this slot means e is a stale handle.
        ACORN_ASSERT_MSG(sparse_[e.index] == kAbsent || dense_entities_[sparse_[e.index]] == e,
                         "emplace with a stale entity handle");

        // Overwrite policy, if the entity already has the component we overwrite it
        if (has(e))
        {
            auto pos = sparse_[e.index];
            dense_data_[pos] = T(std::forward<Args>(args)...);

#ifndef NDEBUG
            debug_check_slot(pos);
#endif
            return dense_data_[pos];
        }
        else
        {
            const auto pos = static_cast<uint32_t>(dense_data_.size());

            dense_entities_.push_back(e);
            dense_data_.emplace_back(std::forward<Args>(args)...);
            sparse_[e.index] = pos;

#ifndef NDEBUG
            debug_check_slot(pos);
#endif
            return dense_data_.back();
        }
    }

    bool remove(Entity e) noexcept
    {
        if (!has(e))
            return false;
        auto pos = sparse_[e.index];
        auto last = static_cast<uint32_t>(dense_data_.size() - 1);

        if (pos != last)
        {
            const Entity moved = dense_entities_[last];

            dense_data_[pos] = std::move(dense_data_[last]);
            dense_entities_[pos] = moved;
            sparse_[moved.index] = pos;
        }

        sparse_[e.index] = kAbsent;

        dense_data_.pop_back();
        dense_entities_.pop_back();

#ifndef NDEBUG
        ACORN_ASSERT(sparse_[e.index] == kAbsent);
        if (pos < dense_entities_.size())
            debug_check_slot(pos); // the entity moved into the hole
#ifdef ACORN_FULL_INVARIANT_CHECKS
        else
            debug_check_invariants();
#endif
#endif
        return true;
    }

    size_t size() const noexcept
    {
        return dense_data_.size();
    }

    bool empty() const noexcept
    {
        return dense_data_.empty();
    }

    size_t capacity() const noexcept
    {
        return dense_data_.capacity();
    }

    void clear() noexcept
    {
        dense_entities_.clear();
        dense_data_.clear();
        std::fill(sparse_.begin(), sparse_.end(), kAbsent);
    }

    auto begin() noexcept
    {
        return dense_data_.begin();
    }

    auto end() noexcept
    {
        return dense_data_.end();
    }

    auto begin() const noexcept
    {
        return dense_data_.begin();
    }

    auto end() const noexcept
    {
        return dense_data_.end();
    }

    auto cbegin() const noexcept
    {
        return dense_data_.cbegin();
    }

    auto cend() const noexcept
    {
        return dense_data_.cend();
    }

private:
#ifndef NDEBUG
    // After each change, debug builds check the slots it touched: the dense entry and its back
    // pointer, and the dense and sparse arrays still the same length. That is constant time, so
    // pools of tens of thousands stay quick to fill. Define ACORN_FULL_INVARIANT_CHECKS to walk
    // the whole pool each time as well (the library's own tests do).
    void debug_check_slot(size_t pos) const
    {
        ACORN_ASSERT(dense_entities_.size() == dense_data_.size());
        const Entity e = dense_entities_[pos];
        ACORN_ASSERT(e.index < sparse_.size());
        ACORN_ASSERT(sparse_[e.index] == pos);
#ifdef ACORN_FULL_INVARIANT_CHECKS
        debug_check_invariants();
#endif
    }

    void debug_check_invariants() const
    {
        ACORN_ASSERT(dense_entities_.size() == dense_data_.size());

        const auto n = dense_entities_.size();

        for (size_t i = 0; i < n; ++i)
        {
            const Entity e = dense_entities_[i];

            // No index bigger than the size
            ACORN_ASSERT(e.index < sparse_.size());

            // Back pointer must point to the dense position
            ACORN_ASSERT(sparse_[e.index] == i);
        }

        // Checking that either are absent or they are using the same index
        for (size_t idx = 0; idx < sparse_.size(); ++idx)
        {
            const uint32_t pos = sparse_[idx];
            if (pos == kAbsent)
                continue;

            ACORN_ASSERT(pos < n);
            ACORN_ASSERT(dense_entities_[pos].index == idx);
        }
    }
#endif
    void grow_sparse_to_fit(uint32_t index)
    {
        if (index >= sparse_.size())
        {
            sparse_.resize(index + 1, kAbsent);
        }
    }

    std::vector<Entity> dense_entities_;
    std::vector<T> dense_data_;
    std::vector<uint32_t> sparse_;

    static constexpr uint32_t kAbsent = UINT32_MAX;
};

}  // namespace acorn