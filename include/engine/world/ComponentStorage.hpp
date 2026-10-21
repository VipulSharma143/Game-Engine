#pragma once
// Sparse-set storage for one component type: O(1) add/remove/lookup, dense iteration.

#include <cstdint>
#include <utility>
#include <vector>

#include "engine/world/Entity.hpp"

namespace ge {

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void remove(std::uint32_t entity_index) = 0;
};

template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
    bool has(std::uint32_t idx) const noexcept {
        return idx < sparse_.size() && sparse_[idx] != kNone;
    }

    T* get(std::uint32_t idx) noexcept { return has(idx) ? &dense_values_[sparse_[idx]] : nullptr; }
    const T* get(std::uint32_t idx) const noexcept { return has(idx) ? &dense_values_[sparse_[idx]] : nullptr; }

    T& add(std::uint32_t idx, T value) {
        if (T* existing = get(idx)) { *existing = std::move(value); return *existing; }
        if (idx >= sparse_.size()) sparse_.resize(idx + 1, kNone);
        sparse_[idx] = static_cast<std::uint32_t>(dense_values_.size());
        dense_owner_.push_back(idx);
        dense_values_.push_back(std::move(value));
        return dense_values_.back();
    }

    void remove(std::uint32_t idx) override {
        if (!has(idx)) return;
        const std::uint32_t slot = sparse_[idx];
        const std::uint32_t last_owner = dense_owner_.back();
        dense_values_[slot] = std::move(dense_values_.back());
        dense_owner_[slot] = last_owner;
        sparse_[last_owner] = slot;
        dense_values_.pop_back();
        dense_owner_.pop_back();
        sparse_[idx] = kNone;
    }

    std::size_t size() const noexcept { return dense_values_.size(); }
    // Entity indices that own this component, parallel to values().
    const std::vector<std::uint32_t>& owners() const noexcept { return dense_owner_; }

private:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;
    std::vector<std::uint32_t> sparse_;
    std::vector<std::uint32_t> dense_owner_;
    std::vector<T> dense_values_;
};

}  // namespace ge
