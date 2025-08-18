#pragma once
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

#include "acorn_assert.hpp"
#include "component_pool.hpp"
#include "entity.hpp"
#include "entity_manager.hpp"

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
        return em_.destroy(e);
    }

    template <class T>
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

    template <class T>
    const ComponentPool<T>& pool() const
    {
        const auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        ACORN_ASSERT_MSG(it != pools_.end(), "const pool<T>() called before creation");
        return static_cast<const PoolBox<T>*>(it->second.get())->pool;
    }

    template <class T>
    bool has(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->has(e);
        return false;
    }

    template <class T>
    T* try_get(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->try_get(e);
        return nullptr;
    }

    template <class T>
    const T* try_get(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->try_get(e);
        return nullptr;
    }

    template <class T>
    T& get(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->get(e);
        throw std::logic_error("get<T> on entity without pool");
    }

    template <class T>
    const T& get(Entity e) const
    {
        if (const auto* p = try_pool<T>())
            return p->get(e);
        throw std::logic_error("get<T> on entity without pool");
    }

    template <class T, class... A>
    T& add(Entity e, A&&... args)
    {
        return pool<T>().emplace(e, std::forward<A>(args)...);
    }

    template <class T>
    bool remove(Entity e)
    {
        if (auto* p = try_pool<T>())
            return p->remove(e);
        return false;
    }

private:
    template <class T>
    ComponentPool<T>* try_pool() noexcept
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end())
            return nullptr;
        return &static_cast<PoolBox<T>*>(it->second.get())->pool;
    }

    template <class T>
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
    };

    template <class T>
    struct PoolBox final : IPool
    {
        ComponentPool<T> pool;

        explicit PoolBox(const EntityManager& em) : pool(em) {}
    };

    EntityManager em_;
    std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools_;
};
}  // namespace acorn