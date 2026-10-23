#pragma once

#include "engine/world/World.hpp"

namespace game {

// Component marking a collectible entity (together with Transform).
struct Pickup {
    ge::PickupKind kind = ge::PickupKind::Health;
};

}  // namespace game
