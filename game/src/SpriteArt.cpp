#include "game/SpriteArt.hpp"

#include <cstdint>
#include <string_view>
#include <utility>

namespace game {

namespace {
#include "SpriteArt.inc"

template <std::size_t R, std::size_t P>
ge::SpriteImage build(const std::string_view (&rows)[R], const std::pair<char, std::uint32_t> (&palette)[P]) {
    return ge::SpriteImage::from_ascii(rows, palette);
}
}  // namespace

SpriteArt SpriteArt::make() {
    SpriteArt a;
    a.enemy[static_cast<int>(ge::EnemyKind::Grunt)][0] = build(kGruntRows, kGruntPalette);
    a.enemy[static_cast<int>(ge::EnemyKind::Gunner)][0] = build(kGunnerRows, kGunnerPalette);
    a.enemy[static_cast<int>(ge::EnemyKind::Brute)][0] = build(kBruteRows, kBrutePalette);
    a.enemy[static_cast<int>(ge::EnemyKind::Skitter)][0] = build(kSkitterRows, kSkitterPalette);
    for (auto& frames : a.enemy) frames[1] = frames[0].mirrored();
    a.shotgun_icon = build(kShotgunIconRows, kShotgunIconPalette);
    a.chaingun_icon = build(kChaingunIconRows, kChaingunIconPalette);
    a.launcher_icon = build(kLauncherIconRows, kLauncherIconPalette);
    return a;
}

const ge::SpriteImage* SpriteArt::pickup_icon(ge::PickupKind kind) const noexcept {
    switch (kind) {
        case ge::PickupKind::Shotgun: return &shotgun_icon;
        case ge::PickupKind::Chaingun: return &chaingun_icon;
        case ge::PickupKind::Launcher: return &launcher_icon;
        default: return nullptr;
    }
}

}  // namespace game
