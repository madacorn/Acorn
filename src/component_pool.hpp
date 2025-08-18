#pragma once
#include <cstdint>
#include <vector>

#include "acorn_assert.hpp"
#include "entity_manager.hpp"

namespace acorn
{
template <class T>
class ComponentPool
{
public:
    explicit ComponentPool(const EntityManager& em) noexcept : em_(em) {}

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

    T* try_get(Entity e)
    {
        if (!has(e))
            return nullptr;

        return &dense_data_[sparse_[e.index]];
    }

    T get(Entity e)
    {
        auto* p = try_get(e);
        ACORN_ASSERT(p);
        return *p;
    }

    template <class... Args>
    T& emplace(Entity e, Args&&... args)
    {
        ACORN_ASSERT(em_.is_alive(e));

        grow_sparse_to_fit(e.index);

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
            dense_data_.emplace_back(T(std::forward<Args>(args)...));
            sparse_[e.index] = pos;

#ifndef NDEBUG
            debug_check_invariants();
#endif
            return dense_data_.back();
        }
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