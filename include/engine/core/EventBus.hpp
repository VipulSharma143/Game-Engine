#pragma once
// Minimal typed publish/subscribe bus. Handlers run synchronously, in subscription order,
// on the thread that calls publish(). Handlers may subscribe/unsubscribe while being invoked.

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ge {

class EventBus {
public:
    using Token = std::size_t;

    template <typename Event>
    Token subscribe(std::function<void(const Event&)> handler) {
        auto& list = channel<Event>();
        const Token token = ++next_token_;
        list.push_back({token, std::move(handler)});
        return token;
    }

    template <typename Event>
    void unsubscribe(Token token) {
        auto& list = channel<Event>();
        list.erase(std::remove_if(list.begin(), list.end(), [token](const Entry<Event>& e) { return e.token == token; }), list.end());
    }

    template <typename Event>
    void publish(const Event& event) {
        auto& list = channel<Event>();
        const auto snapshot = list;  // handlers may change the subscriber list
        for (const auto& entry : snapshot) entry.handler(event);
    }

    template <typename Event>
    std::size_t subscriber_count() {
        return channel<Event>().size();
    }

private:
    template <typename Event>
    struct Entry { Token token; std::function<void(const Event&)> handler; };

    struct ChannelBase { virtual ~ChannelBase() = default; };
    template <typename Event>
    struct Channel final : ChannelBase { std::vector<Entry<Event>> entries; };

    template <typename Event>
    std::vector<Entry<Event>>& channel() {
        auto& slot = channels_[std::type_index(typeid(Event))];
        if (!slot) slot = std::make_unique<Channel<Event>>();
        return static_cast<Channel<Event>&>(*slot).entries;
    }

    std::unordered_map<std::type_index, std::unique_ptr<ChannelBase>> channels_;
    Token next_token_ = 0;
};

}  // namespace ge
