#pragma once

#include <string>
#include <utility>

namespace ge {

// Lightweight success/failure result carrying a human-readable message.
struct Status {
    bool ok = true;
    std::string message;

    static Status success() { return {}; }
    static Status failure(std::string text) { return {false, std::move(text)}; }
    explicit operator bool() const noexcept { return ok; }
};

}  // namespace ge
