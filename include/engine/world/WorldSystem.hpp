#pragma once
// Small entity/component registry (no external ECS dependency) and the stock components.

#include <cstddef>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "engine/core/Math.hpp"
#include "engine/world/ComponentStorage.hpp"
#include "engine/world/Entity.hpp"

namespace ge {

struct Transform { Vec2 position{}; double angle = 0.0; };
struct Velocity { Vec2 linear{}; };
struct Collider { double radius = 0.25; };
struct Health { int current = 100; int max = 100; bool dead() const noexcept { return current <= 0; } };

class EntityWorld {
public:
    Entity create() {
        std::uint32_t idx;
        if (!free_.empty()) { idx = free_.back(); free_.pop_back(); }
        else { idx = static_cast<std::uint32_t>(generations_.size()); generations_.push_back(0); alive_.push_back(false); }
        alive_[idx] = true;
        ++alive_count_;
        return {idx, generations_[idx]};
    }

    bool alive(Entity e) const noexcept {
        return e.index < generations_.size() && alive_[e.index] && generations_[e.index] == e.generation;
    }

    std::size_t size() const noexcept { return alive_count_; }

    void destroy(Entity e) {
        if (!alive(e)) return;
        for (auto& kv : storages_) kv.second->remove(e.index);
        alive_[e.index] = false;
        ++generations_[e.index];
        free_.push_back(e.index);
        --alive_count_;
    }

    template <typename T>
    T& add(Entity e, T value = {}) { return storage<T>().add(e.index, std::move(value)); }

    template <typename T>
    void remove(Entity e) { if (alive(e)) storage<T>().remove(e.index); }

    template <typename T>
    bool has(Entity e) const noexcept { return alive(e) && storage_if<T>() && storage_if<T>()->has(e.index); }

    template <typename T>
    T* get(Entity e) noexcept { return alive(e) ? storage<T>().get(e.index) : nullptr; }

    // Calls fn(Entity, A&, B&...) for every live entity that has all listed components.
    template <typename A, typename... Rest, typename Fn>
    void each(Fn&& fn) {
        auto& primary = storage<A>();
        // Copy owners: callbacks may add/remove components.
        const std::vector<std::uint32_t> owners = primary.owners();
        for (std::uint32_t idx : owners) {
            if (!(storage<Rest>().has(idx) && ...)) continue;
            A* a = primary.get(idx);
            if (!a) continue;
            fn(Entity{idx, generations_[idx]}, *a, *storage<Rest>().get(idx)...);
        }
    }

private:
    template <typename T>
    ComponentStorage<T>& storage() {
        auto& slot = storages_[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<ComponentStorage<T>>();
        return static_cast<ComponentStorage<T>&>(*slot);
    }
    template <typename T>
    const ComponentStorage<T>* storage_if() const {
        auto it = storages_.find(std::type_index(typeid(T)));
        return it == storages_.end() ? nullptr : static_cast<const ComponentStorage<T>*>(it->second.get());
    }

    std::vector<std::uint32_t> generations_;
    std::vector<bool> alive_;
    std::vector<std::uint32_t> free_;
    std::size_t alive_count_ = 0;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages_;
};

}  // namespace ge
