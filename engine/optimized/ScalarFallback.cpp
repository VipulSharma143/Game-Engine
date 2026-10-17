#include "engine/optimized/ScalarFallback.hpp"

namespace ge::scalar {

std::int64_t add_i64(std::int64_t a, std::int64_t b) noexcept {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b));
}

std::int64_t multiply_i64(std::int64_t a, std::int64_t b) noexcept {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
}

}  // namespace ge::scalar
