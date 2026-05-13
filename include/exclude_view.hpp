#pragma once
#include "view.hpp"

namespace acorn
{

template <typename... Excluded>
struct Exclude
{
};

template <typename IncludePools, typename ExcludePools>
class ExcludeView;

template <typename... Include, typename... Exclude>
class ExcludeView<std::tuple<Include...>, std::tuple<Exclude...>>
{
public:
    ExcludeView(std::tuple<Include&...> include, std::tuple<Exclude&...> exclude)
        : include_(include), exclude_(exclude)
    {
        // pick smallest include pool as the lead — same logic as View
        size_t min_size = std::numeric_limits<size_t>::max();
        auto find_smallest = [&](auto& pool)
        {
            if (pool.size() < min_size)
            {
                min_size = pool.size();
                lead_entities_ = &pool.entities();
            }
        };
        std::apply([&](auto&... pools) { (find_smallest(pools), ...); }, include_);
    }

    bool contains_all(Entity e) const
    {
        return std::apply([e](auto&... pools) { return (pools.has(e) && ...); }, include_);
    }

    bool contains_any_excluded(Entity e) const
    {
        return std::apply([e](auto&... pools) { return (pools.has(e) || ...); }, exclude_);
    }

    template <typename Func>
    void each(Func&& f) const
    {
        if (!lead_entities_)
            return;

        for (Entity e : *lead_entities_)
        {
            if (contains_all(e) && !contains_any_excluded(e))
            {
                std::apply([&](auto&... pools) { f(e, pools.get(e)...); }, include_);
            }
        }
    }

private:
    std::tuple<Include&...> include_;
    std::tuple<Exclude&...> exclude_;
    const std::vector<Entity>* lead_entities_ = nullptr;
};

}  // namespace acorn