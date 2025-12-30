#pragma once
#include <iterator>
#include <limits>
#include <tuple>
#include <vector>

#include "component_pool.hpp"
#include "entity.hpp"

namespace acorn
{
template <typename... Pools>
class View
{
public:
    View(Pools&... pools) : pools_(std::forward_as_tuple(pools...))
    {
        size_t min_size = std::numeric_limits<size_t>::max();

        auto find_smallest = [&](auto& pool)
        {
            if (pool.size() < min_size)
            {
                min_size = pool.size();
                lead_entities_ = &pool.entities();
            }
        };
        (find_smallest(pools), ...);
    }

    bool contains_all(Entity e) const
    {
        return std::apply([e](auto&... pools) { return (pools.has(e) && ...); }, pools_);
    }

    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Entity;
        using difference_type = std::ptrdiff_t;
        using pointer = Entity*;
        using reference = Entity&;

        Iterator(const View& view, size_t index) : view_(view), index_(index)
        {
            move_to_valid();
        }

        Entity operator*() const
        {
            return (*view_.lead_entities_)[index_];
        }

        Iterator& operator++()
        {
            index_++;
            move_to_valid();
            return *this;
        }

        bool operator==(const Iterator& other) const
        {
            return index_ == other.index_;
        }

        bool operator!=(const Iterator& other) const
        {
            return index_ != other.index_;
        }

    private:
        void move_to_valid()
        {
            while (view_.lead_entities_ && index_ < view_.lead_entities_->size() &&
                   !view_.contains_all((*view_.lead_entities_)[index_]))
            {
                index_++;
            }
        }

        const View& view_;
        size_t index_;
    };

    [[nodiscard]] Iterator begin() const
    {
        return Iterator{*this, 0};
    }

    [[nodiscard]] Iterator end() const
    {
        return Iterator{*this, lead_entities_ ? lead_entities_->size() : 0};
    }

private:
    std::tuple<Pools&...> pools_;
    const std::vector<Entity>* lead_entities_ = nullptr;
};
}  // namespace acorn