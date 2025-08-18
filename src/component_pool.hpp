#pragma once
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "acorn_assert.hpp"
#include "entity_manager.hpp"

namespace acorn
{
template <class T>
class ComponentPool
{
public:
    explicit ComponentPool(const EntityManager& em, size_t reserve_hint = 0) noexcept : em_(em)
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
        if (!em_.is_alive(e))
            return false;
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
        throw std::logic_error("get<T> on entity without component");
    }

    [[nodiscard]] const T& get(Entity e) const
    {
        if (auto* p = try_get(e))
            return *p;
        throw std::logic_error("get<T> on entity without component");
    }

    template <class... Args>
    T& emplace(Entity e, Args&&... args)
    {
        ACORN_ASSERT(em_.is_alive(e));

        grow_sparse_to_fit(e.index);

        // Overwrite policy, if the entity already has the component we overwrite it
        if (has(e))
        {
            auto pos = sparse_[e.index];
            dense_data_[pos] = T(std::forward<Args>(args)...);

#ifndef NDEBUG
            debug_check_invariants();
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
            debug_check_invariants();
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
        debug_check_invariants();
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
    void debug_check_invariants() const
    {
        ACORN_ASSERT(dense_entities_.size() == dense_data_.size());

        const auto n = dense_entities_.size();

        for (size_t i = 0; i < n; ++i)
        {
            const Entity e = dense_entities_[i];

            // Must be alive
            ACORN_ASSERT(em_.is_alive(e));

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

    const EntityManager& em_;

    std::vector<Entity> dense_entities_;
    std::vector<T> dense_data_;
    std::vector<uint32_t> sparse_;

    static constexpr uint32_t kAbsent = UINT32_MAX;
};

}  // namespace acorn