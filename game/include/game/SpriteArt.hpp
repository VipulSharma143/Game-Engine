#pragma once
// The game's pixel art, built once from rows of text (see SpriteArt.inc): one picture per enemy kind (two walking
// frames) and one icon per weapon pickup.

#include "engine/renderer/SpriteImage.hpp"
#include "engine/world/World.hpp"

namespace game {

struct SpriteArt {
    ge::SpriteImage enemy[ge::kEnemyKindCount][2];  // [kind][frame]; frame 1 is frame 0 mirrored, so walking shows
    ge::SpriteImage shotgun_icon, chaingun_icon, launcher_icon;

    static SpriteArt make();
    // The icon for a weapon pickup, or nullptr for pickups drawn as plain gems.
    const ge::SpriteImage* pickup_icon(ge::PickupKind kind) const noexcept;
};

}  // namespace game
