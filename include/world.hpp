#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "acorn_assert.hpp"
#include "component_pool.hpp"
#include "entity.hpp"
#include "entity_manager.hpp"
#include "exclude_view.hpp"
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

    using DestroyListener = std::function<void(World&, Entity)>;

    // Registers a listener called whenever an entity is destroyed (by destroy_entity, or by
    // flush() for defer_destroy). Listeners run in registration order, before the entity's
    // components are removed, so they can still read them. clear() does not fire listeners.
    // Must not be called from inside a listener.
    void on_destroy(DestroyListener listener)
    {
        ACORN_ASSERT_MSG(!destroying_, "on_destroy called from inside a destroy listener");
        destroy_listeners_.push_back(std::move(listener));
    }

    // Returns false if e is not alive. When called from inside a destroy listener, the
    // destruction is queued and performed (firing listeners again) after the current entity
    // is fully destroyed, in call order.
    bool destroy_entity(Entity e)
    {
        if (!em_.is_alive(e))
            return false;

        if (destroy_listeners_.empty())
        {
            remove_entity(e);
            return true;
        }

        if (destroying_)
        {
            pending_destroys_.push_back(e);
            return true;
        }

        DestroyScope scope{*this};
        notify_and_remove(e);
        // Index loop: listeners may queue more destructions while we drain.
        for (size_t i = 0; i < pending_destroys_.size(); ++i)
        {
            const Entity pending = pending_destroys_[i];
            if (em_.is_alive(pending))
                notify_and_remove(pending);
        }
        return true;
    }

    [[nodiscard]] bool is_alive(Entity e) const noexcept
    {
        return em_.is_alive(e);
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

    // A const view over a component type that has no pool yet is empty rather than throwing.
    template <typename... Components>
    [[nodiscard]] const auto view() const
    {
        return View{pool_or_empty<Components>()...};
    }

    template <typename... Components, typename... Excluded>
    [[nodiscard]] auto view_exclude(acorn::Exclude<Excluded...> = {})
    {
        return ExcludeView<std::tuple<ComponentPool<Components>...>,
                           std::tuple<ComponentPool<Excluded>...>>(
            std::forward_as_tuple(pool<Components>()...),  // include pools
            std::forward_as_tuple(pool<Excluded>()...)     // exclude pools
        );
    }

    template <typename... Components, typename... Excluded>
    [[nodiscard]] const auto view_exclude(acorn::Exclude<Excluded...> = {}) const
    {
        return ExcludeView<std::tuple<const ComponentPool<Components>...>,
                           std::tuple<const ComponentPool<Excluded>...>>(
            std::forward_as_tuple(pool_or_empty<Components>()...),
            std::forward_as_tuple(pool_or_empty<Excluded>()...));
    }

    // Bulk reset: removes every entity and component without firing destroy listeners.
    void clear()
    {
        ACORN_ASSERT_MSG(!destroying_, "clear called from inside a destroy listener");
        for (auto& [_, pool_ptr] : pools_)
        {
            pool_ptr->clear();
        }

        em_.reset();
    }

    // --- Serialization hooks -------------------------------------------------------------
    // Save: export_entities(), then each_in_order<T>() for every component type.
    // Load: on an empty (fresh or cleared) world, import_entities(), then restore_component<T>()
    // in the same order. Views then iterate in the same order and create_entity() returns the
    // same next handle as on the original world. flush() pending commands before saving.

    [[nodiscard]] EntityState export_entities() const
    {
        return em_.export_state();
    }

    // Throws std::logic_error if the world has any entity slots or components (call clear()
    // first), and std::invalid_argument if the state is malformed.
    void import_entities(const EntityState& state)
    {
        if (em_.capacity() != 0)
            throw std::logic_error("acorn::World::import_entities: world is not empty");
        for (const auto& [_, pool_ptr] : pools_)
        {
            if (pool_ptr->size() != 0)
                throw std::logic_error("acorn::World::import_entities: world is not empty");
        }
        em_.import_state(state);
    }

    // Visits every T in dense (iteration) order as fn(Entity, const T&).
    template <typename T, typename Func>
    void each_in_order(Func&& fn) const
    {
        const auto* p = try_pool<T>();
        if (!p)
            return;

        const auto& entities = p->entities();
        auto it = p->begin();
        for (size_t i = 0; i < entities.size(); ++i, ++it)
        {
            fn(entities[i], *it);
        }
    }

    // Appends component T for e, so calls in each_in_order<T>() order rebuild the same dense
    // order. Throws std::logic_error if e is not alive or already has T.
    template <typename T>
    T& restore_component(Entity e, T component)
    {
        if (!em_.is_alive(e))
            throw std::logic_error("acorn::World::restore_component: entity is not alive");
        auto& p = pool<T>();
        if (p.has(e))
            throw std::logic_error("acorn::World::restore_component: entity already has component");
        return p.emplace(e, std::move(component));
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

    // Commands deferred while flushing (e.g. by destroy listeners) run in the same flush.
    void flush()
    {
        while (!commands_.empty())
        {
            auto batch = std::move(commands_);
            commands_.clear();
            for (auto& cmd : batch)
            {
                cmd(*this);
            }
        }
    }

private:
    void remove_entity(Entity e)
    {
        for (auto& [type, pool_ptr] : pools_)
        {
            pool_ptr->remove(e);
        }
        em_.destroy(e);
    }

    void notify_and_remove(Entity e)
    {
        for (auto& listener : destroy_listeners_)
        {
            listener(*this, e);
        }
        remove_entity(e);
    }

    struct DestroyScope
    {
        World& w;

        explicit DestroyScope(World& world) : w(world)
        {
            w.destroying_ = true;
        }

        ~DestroyScope()
        {
            w.pending_destroys_.clear();
            w.destroying_ = false;
        }
    };

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

    // Returns the registered pool, or a shared empty pool when T has never been used.
    template <typename T>
    const ComponentPool<T>& pool_or_empty() const
    {
        if (const auto* p = try_pool<T>())
            return *p;

        static const EntityManager empty_em;
        static const ComponentPool<T> empty_pool(empty_em);
        return empty_pool;
    }

    struct IPool
    {
        virtual ~IPool() = default;
        virtual bool remove(Entity e) noexcept = 0;
        virtual void clear() noexcept = 0;
        virtual size_t size() const noexcept = 0;
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

        void clear() noexcept override
        {
            pool.clear();
        }

        size_t size() const noexcept override
        {
            return pool.size();
        }
    };

    EntityManager em_;
    std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools_;
    std::vector<std::function<void(World&)>> commands_;
    std::vector<DestroyListener> destroy_listeners_;
    std::vector<Entity> pending_destroys_;
    bool destroying_ = false;
};
}  // namespace acorn