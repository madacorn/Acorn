#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

#include "acorn_assert.hpp"
#include "component_pool.hpp"
#include "entity.hpp"
#include "entity_manager.hpp"
#include "view.hpp"

namespace acorn
{
class World
{
public:
    Entity create_entity()
    {
        return em_.create();
    }

    bool destroy_entity(Entity e)
    {
        if (!em_.is_alive(e))
            return false;

        for (auto& [type, pool_ptr] : pools_)
        {
            pool_ptr->remove(e);
        }
        return em_.destroy(e);
    }

    template <typename T>
    ComponentPool<T>& pool()
    {
        const auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end())
        {
            auto box = std::make_unique<PoolBox<T>>(em_);
            auto* out = &box->pool;
            pools_.emplace(key, std::move(box));
            return *out;
        }
        return static_cast<PoolBox<T>*>(it->second.get())->pool;
    }

    template <typename T>
    const ComponentPool<T>& pool() const
    {
        const auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end())
        {
            throw std::runtime_error(
                "ComponentPool requested for type not yet registered in World.");
        }
        return static_cast<const PoolBox<T>*>(it->second.get())->pool;
    }

    template <typename T>
    bool has(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->has(e);
        return false;
    }

    template <typename T>
    T* try_get(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->try_get(e);
        return nullptr;
    }

    template <typename T>
    const T* try_get(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->try_get(e);
        return nullptr;
    }

    template <typename T>
    T& get(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->get(e);
        throw std::out_of_range("acorn::World: entity does not have the requested pool");
    }

    template <typename T>
    const T& get(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->get(e);
        throw std::out_of_range("acorn::World: entity does not have the requested pool");
    }

    template <typename T, typename... A>
    T& add(Entity e, A&&... args)
    {
        return pool<T>().emplace(e, std::forward<A>(args)...);
    }

    template <typename T>
    bool remove(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->remove(e);
        return false;
    }

    template <typename... Components>
    [[nodiscard]] auto view()
    {
        return View{(pool<Components>())...};
    }

    template <typename... Components>
    [[nodiscard]] const auto view() const
    {
        return View{pool<Components>()...};
    }

    template <typename T>
    void defer_remove(Entity e)
    {
        commands_.emplace_back([e](World& w) { w.remove<T>(e); });
    }

    void defer_destroy(Entity e)
    {
        commands_.emplace_back([e](World& w) { w.destroy_entity(e); });
    }

    void flush()
    {
        for (auto& cmd : commands_)
        {
            cmd(*this);
        }
        commands_.clear();
    }

private:
    template <typename T>
    ComponentPool<T>* try_pool() noexcept
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end())
            return nullptr;
        return &static_cast<PoolBox<T>*>(it->second.get())->pool;
    }

    template <typename T>
    const ComponentPool<T>* try_pool() const noexcept
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end())
            return nullptr;
        return &static_cast<const PoolBox<T>*>(it->second.get())->pool;
    }

    struct IPool
    {
        virtual ~IPool() = default;
        virtual bool remove(Entity e) noexcept = 0;
    };

    template <typename T>
    struct PoolBox final : IPool
    {
        ComponentPool<T> pool;

        explicit PoolBox(const EntityManager& em) : pool(em) {}

        bool remove(Entity e) noexcept override
        {
            return pool.remove(e);
        }
    };

    EntityManager em_;
    std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools_;
    std::vector<std::function<void(World&)>> commands_;
};
}  // namespace acorn