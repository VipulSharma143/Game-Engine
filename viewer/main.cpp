// SORTED Viewer: the engine's own program. It opens a window, loads a level and lets you walk around it,
// shoot test orbs, switch levels and look at the engine's statistics. It uses only the public SORTED API
// (no code from game/), so it is also a worked example of a program built on the engine.
//
//   WASD / arrows  move and turn      mouse  look            click / Space  shoot an orb
//   E  next level                     R  textures on/off     F3  statistics
//   Esc  pause and help               Q  quit (while paused)
//   F2  edit mode: left click select, right click insert, wheel or [ ] choose what to insert, X/Delete remove,
//       M move, C change, , . rotate the player start, Backspace deselect, F5 save, F9 reload from the file
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/Engine.hpp"
#include "engine/core/Settings.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/physics/Movement.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "engine/platform/PlatformFactory.hpp"
#include "engine/platform/ScriptedInput.hpp"
#include "engine/renderer/Raycaster.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/renderer/TextRenderer.hpp"
#include "engine/resources/AssetPath.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/world/LevelEditor.hpp"
#include "engine/world/LevelLoader.hpp"

namespace {

constexpr double kWalkSpeed = 3.2;     // tiles per second
constexpr double kTurnSpeed = 2.4;     // radians per second (keyboard)
constexpr double kBodyRadius = 0.25;   // collision radius, in tiles
constexpr double kOrbSpeed = 10.0;
constexpr double kOrbCooldown = 0.16;
constexpr std::size_t kMaxOrbs = 48;
constexpr double kHitRadius = 0.35;
constexpr double kHelpSeconds = 6.0;
constexpr double kEditReach = 3.0;       // tiles: how far ahead an insert or move lands when no wall is nearer
constexpr double kMessageSeconds = 3.0;  // how long an edit result stays on screen

constexpr std::uint32_t kWhite = 0xFFFFFFFFu;
constexpr std::uint32_t kYellow = 0xFFFFD84Au;
constexpr std::uint32_t kGrey = 0xFFB4BCC8u;
constexpr std::uint32_t kRed = 0xFFFF6B6Bu;
constexpr std::uint32_t kPlayerMarker = 0xFFE040E0u;
// Minimap colours by wall id (0 = floor, 10 = door, 11 = locked door).
constexpr std::uint32_t kMapColors[18] = {0xFF2A2A2Au, 0xFFC83C3Cu, 0xFF3CBE3Cu, 0xFF465ADCu, 0xFFD2BE3Cu, 0xFF3CBEBEu,
                                          0xFFBE46BEu, 0xFFE6E6E6u, 0xFFE68C32u, 0xFF8C5A32u, 0xFF78501Eu, 0xFFAA2828u,
                                          0xFF2E8B57u, 0xFFD4C020u, 0xFF6B8E4Eu, 0xFFA0522Du, 0xFFFF4500u, 0xFF7FD4FFu};

struct Orb {
    ge::Vec2 position;
    ge::Vec2 velocity;
    bool stuck = false;  // hit a wall and stays there as a mark
};

struct Marker {
    ge::Vec2 position;
    std::uint32_t color;
    double size;
    bool diamond;
    bool shootable;
    double flash = 0.0;
};

void fill_rect(ge::Framebuffer& fb, int x, int y, int w, int h, std::uint32_t color) {
    for (int cx = std::max(0, x); cx < std::min(fb.width(), x + w); ++cx)
        fb.fill_column(cx, std::max(0, y), std::min(fb.height(), y + h), color);
}

void centered(ge::Framebuffer& fb, int y, const std::string& text, std::uint32_t color) {
    ge::draw_text_shadowed(fb, (fb.width() - ge::text_width(text)) / 2, y, text, color);
}

void frame_rect(ge::Framebuffer& fb, int x, int y, int w, int h, std::uint32_t color) {
    fill_rect(fb, x, y, w, 1, color); fill_rect(fb, x, y + h - 1, w, 1, color);
    fill_rect(fb, x, y, 1, h, color); fill_rect(fb, x + w - 1, y, 1, h, color);
}

class Viewer final : public ge::Application {
public:
    // `paths[i]` is the file levels[i] was read from (empty for the built-in level): where F5 saves it.
    Viewer(std::vector<ge::Level> levels, std::vector<std::filesystem::path> paths, std::size_t start, bool textured)
        : levels_(std::move(levels)), paths_(std::move(paths)), start_(start), textured_(textured) {}

    ge::Status initialize(const ge::EngineConfig& config) override {
        config_ = config;
        textures_ = ge::TextureSet::make_procedural();
        renderer_.set_textures(textured_ ? &textures_ : nullptr);
        enter_level(start_ < levels_.size() ? start_ : 0);
        last_frame_ = Clock::now();
        return ge::Status::success();
    }

    void fixed_update(const ge::InputState& in, double dt) override {
        if (in.quit) quit_ = true;
        elapsed_ += dt;
        if (in.pause) paused_ = !paused_;
        if (paused_) {
            if (in.menu_quit) quit_ = true;
            return;
        }
        if (in.toggle_debug) debug_ = !debug_;
        if (in.edit_toggle) {
            edit_ = !edit_;
            if (!edit_) renderer_.clear_highlight();
        }
        if (in.use) {
            if (editor_.dirty()) toast(ge::Status::failure("UNSAVED EDITS: F5 SAVE OR F9 DISCARD"));  // never drop edits silently
            else enter_level((index_ + 1) % levels_.size());
        }
        sync_markers();
        if (in.reload) {
            textured_ = !textured_;
            renderer_.set_textures(textured_ ? &textures_ : nullptr);
        }

        angle_ += ge::clamp(in.turn, -1.0, 1.0) * kTurnSpeed * dt + in.look * config_.mouse_sensitivity;
        const ge::Vec2 forward = ge::heading_vector(angle_);
        ge::Vec2 wish = forward * ge::clamp(in.forward, -1.0, 1.0) + ge::right_of(forward) * ge::clamp(in.strafe, -1.0, 1.0);
        const double length = std::sqrt(wish.x * wish.x + wish.y * wish.y);
        if (length > 1.0) wish = wish * (1.0 / length);
        position_ = ge::move_with_collision(level().map, position_, wish * (kWalkSpeed * dt), kBodyRadius);

        message_time_ = std::max(0.0, message_time_ - dt);
        cooldown_ -= dt;
        if (edit_) edit_step(in);
        if (!edit_ && in.fire && cooldown_ <= 0.0) {
            cooldown_ = kOrbCooldown;
            orbs_.push_back({position_ + forward * 0.3, forward * kOrbSpeed, false});
            if (orbs_.size() > kMaxOrbs) orbs_.pop_front();
        }
        update_orbs(dt);
        for (Marker& m : markers_) m.flash = std::max(0.0, m.flash - dt * 3.0);
    }

    void render(ge::Framebuffer& fb) override {
        const auto now = Clock::now();
        const double frame_seconds = std::chrono::duration<double>(now - last_frame_).count();
        last_frame_ = now;
        if (frame_seconds > 0.0) fps_ += (1.0 / frame_seconds - fps_) * 0.1;  // smoothed

        sync_markers();
        const ge::Camera camera = ge::Camera::from_angle(position_, angle_, config_.fov_degrees);
        if (edit_ && editor_.has_selection() && ge::is_solid_object(editor_.selected()))
            renderer_.set_highlight_cell(editor_.selected_x(), editor_.selected_y());
        else renderer_.clear_highlight();
        renderer_.render(level().map, camera, fb);

        sprites_.clear();  // reused every frame; no allocation once it has grown
        const double pulse = 0.5 + 0.4 * std::sin(elapsed_ * 8.0);
        auto add_marker = [&](ge::Vec2 position, double size, std::uint32_t color, double flash, bool diamond) {
            const bool selected = edit_ && editor_.has_selection() && static_cast<int>(std::floor(position.x)) == editor_.selected_x() &&
                                  static_cast<int>(std::floor(position.y)) == editor_.selected_y();
            sprites_.push_back({position, selected ? size * 1.3 : size, color, selected ? pulse : flash, diamond});
        };
        for (const Marker& m : markers_) add_marker(m.position, m.size, m.color, m.flash, m.diamond);
        if (edit_) {
            add_marker(level().spawn, 0.5, kPlayerMarker, 0.0, true);  // the player start is only drawn while editing
            sprites_.push_back({{target_.free_x + 0.5, target_.free_y + 0.5}, 0.22, kWhite, 0.0, true});  // where an insert lands
        }
        for (const Orb& o : orbs_) sprites_.push_back({o.position, o.stuck ? 0.10 : 0.14, o.stuck ? 0xFFB5531Cu : 0xFFFF9A3Cu, 0.0, false});
        ge::draw_sprites(camera, renderer_.depth(), sprites_, fb);

        const int cx = fb.width() / 2, cy = fb.height() / 2;  // crosshair (yellow and longer while editing)
        const std::uint32_t cross = edit_ ? kYellow : kWhite;
        for (int d = 2; d <= (edit_ ? 6 : 4); ++d) {
            fb.set_pixel(cx - d, cy, cross); fb.set_pixel(cx + d, cy, cross);
            fb.set_pixel(cx, cy - d, cross); fb.set_pixel(cx, cy + d, cross);
        }

        ge::draw_text_shadowed(fb, 4, 4, "SORTED VIEWER", kYellow);
        ge::draw_text_shadowed(fb, 4, 14, "LEVEL " + std::to_string(index_ + 1) + "/" + std::to_string(levels_.size()) + ": " + upper(level().name), kWhite);
        ge::draw_text_shadowed(fb, fb.width() - ge::text_width("ESC: HELP") - 4, 4, "ESC: HELP", kGrey);
        if (edit_) draw_edit_hud(fb);
        else if (hits_ > 0) ge::draw_text_shadowed(fb, 4, 24, "HITS: " + std::to_string(hits_), kWhite);
        if (message_time_ > 0.0) centered(fb, fb.height() - 14, message_, message_ok_ ? kWhite : kRed);

        if (debug_) draw_stats(fb);
        if (paused_) draw_help(fb, true);
        else if (elapsed_ < kHelpSeconds) draw_help(fb, false);
    }

    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return quit_; }
    bool wants_mouse_capture() const noexcept override { return !paused_; }

private:
    using Clock = std::chrono::steady_clock;

    const ge::Level& level() const { return editor_.level(); }

    static std::string upper(std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }

    void toast(const ge::Status& status) {
        message_ = upper(status.message);
        message_ok_ = status.ok;
        message_time_ = kMessageSeconds;
    }

    void respawn() {
        position_ = level().spawn;
        angle_ = level().spawn_angle;
    }

    void enter_level(std::size_t index) {
        if (entered_) levels_[index_] = editor_.level();  // keep what was saved if we come back
        entered_ = true;
        index_ = index;
        editor_.load(levels_[index_], index_ < paths_.size() ? paths_[index_] : std::filesystem::path{});
        respawn();
        hits_ = 0;
        sync_markers();
    }

    // The markers (and the orbs stuck to walls) are derived from the level, so rebuild them when it changes.
    void sync_markers() {
        if (marker_revision_ == editor_.revision()) return;
        marker_revision_ = editor_.revision();
        orbs_.clear();
        markers_.clear();
        for (const ge::Vec2& p : level().target_spawns) markers_.push_back({p, 0xFF4FA3FFu, 0.45, false, true});
        for (const ge::EnemySpawn& p : level().enemy_spawns) {
            constexpr std::uint32_t kEnemyColors[ge::kEnemyKindCount] = {0xFFE04040u, 0xFFE0A030u, 0xFF9040C0u, 0xFF40E0A0u};
            markers_.push_back({p.position, kEnemyColors[static_cast<int>(p.kind) % ge::kEnemyKindCount], 0.55, false, true});
        }
        for (const ge::PickupSpawn& p : level().pickups) {
            constexpr std::uint32_t kPickupColors[] = {0xFF4CD964u, 0xFFFFC107u, 0xFF40D0FFu, 0xFFFF8030u, 0xFFC080FFu, 0xFFFF5050u};
            const std::uint32_t color = kPickupColors[static_cast<int>(p.kind)];
            markers_.push_back({p.position, color, 0.30, true, false});
        }
    }

    void edit_step(const ge::InputState& in) {
        const ge::Camera camera = ge::Camera::from_angle(position_, angle_, config_.fov_degrees);
        target_ = ge::crosshair_target(level(), camera, kEditReach);
        editor_.set_player_cell(static_cast<int>(std::floor(position_.x)), static_cast<int>(std::floor(position_.y)));
        editor_.cycle_palette(in.edit_palette);
        if (in.edit_deselect) editor_.deselect();
        if (in.edit_select && target_.hit) editor_.select(target_.cell_x, target_.cell_y);
        if (in.edit_insert) toast(editor_.insert(target_.free_x, target_.free_y));
        if (in.edit_remove) toast(editor_.remove_selected());
        if (in.edit_move) toast(editor_.move_selected(target_.free_x, target_.free_y));
        if (in.edit_change) toast(editor_.change_selected());
        if (in.edit_rotate != 0) toast(editor_.rotate_selected(in.edit_rotate));
        if (in.edit_save) toast(editor_.save());
        if (in.edit_load) {
            const ge::Status reloaded = editor_.reload();
            toast(reloaded);
            if (reloaded) respawn();  // the old position may now be inside a wall
        }
    }

    void draw_edit_hud(ge::Framebuffer& fb) const {
        ge::draw_text_shadowed(fb, 4, 24, editor_.dirty() ? "EDIT MODE - UNSAVED" : "EDIT MODE", kYellow);
        ge::draw_text_shadowed(fb, 4, 34, "INSERT: " + ge::object_name(editor_.palette_current()) + "  (WHEEL)", kWhite);
        std::string selected = "SELECTED: NONE";
        if (editor_.has_selection()) {
            selected = "SELECTED: " + ge::object_name(editor_.selected()) + " (" + std::to_string(editor_.selected_x()) + "," +
                       std::to_string(editor_.selected_y()) + ")";
        }
        ge::draw_text_shadowed(fb, 4, 44, selected, kWhite);
        draw_minimap(fb);
    }

    // Top-down view of the whole level: the editor's overview, with the selection and the insert cursor marked.
    void draw_minimap(ge::Framebuffer& fb) const {
        const ge::GridMap& map = level().map;
        const int cell = std::clamp(96 / std::max(map.width(), map.height()), 1, 6);
        const int w = map.width() * cell, h = map.height() * cell;
        const int x0 = fb.width() - w - 6, y0 = 16;
        fill_rect(fb, x0 - 1, y0 - 1, w + 2, h + 2, 0xFF000000u);
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x)
                fill_rect(fb, x0 + x * cell, y0 + y * cell, cell, cell, kMapColors[std::min<int>(map.cell(x, y), 17)]);
        auto dot = [&](ge::Vec2 p, std::uint32_t color) {
            const int size = std::max(2, cell - 1);
            fill_rect(fb, x0 + static_cast<int>(std::floor(p.x)) * cell + (cell - size) / 2, y0 + static_cast<int>(std::floor(p.y)) * cell + (cell - size) / 2, size, size, color);
        };
        for (const Marker& m : markers_) dot(m.position, m.color);
        dot(level().spawn, kPlayerMarker);
        const int px = x0 + static_cast<int>(position_.x * cell), py = y0 + static_cast<int>(position_.y * cell);
        fill_rect(fb, px - 1, py - 1, 3, 3, kWhite);  // you, with a short line showing where you face
        fill_rect(fb, px + static_cast<int>(std::lround(std::cos(angle_) * 3.0)), py + static_cast<int>(std::lround(std::sin(angle_) * 3.0)), 1, 1, kYellow);
        frame_rect(fb, x0 + target_.free_x * cell - 1, y0 + target_.free_y * cell - 1, cell + 2, cell + 2, kWhite);
        if (editor_.has_selection()) frame_rect(fb, x0 + editor_.selected_x() * cell - 1, y0 + editor_.selected_y() * cell - 1, cell + 2, cell + 2, kYellow);
    }

    void update_orbs(double dt) {
        for (Orb& orb : orbs_) {
            if (orb.stuck) continue;
            const ge::Vec2 next = orb.position + orb.velocity * dt;
            if (level().map.is_solid(static_cast<int>(std::floor(next.x)), static_cast<int>(std::floor(next.y)))) {
                orb.stuck = true;  // leave a mark on the wall
                continue;
            }
            orb.position = next;
            for (Marker& m : markers_) {
                if (!m.shootable) continue;
                const double dx = m.position.x - orb.position.x, dy = m.position.y - orb.position.y;
                if (dx * dx + dy * dy < kHitRadius * kHitRadius) {
                    m.flash = 1.0;
                    ++hits_;
                    orb.stuck = true;
                    orb.position = m.position;
                    break;
                }
            }
        }
    }

    void draw_stats(ge::Framebuffer& fb) const {
        char line[96];
        int y = fb.height() - 50;
        auto put = [&](const char* text) { ge::draw_text_shadowed(fb, 4, y, text, kGrey); y += 9; };
        std::snprintf(line, sizeof line, "FPS: %.0f  FRAME: %.1f MS", fps_, fps_ > 0 ? 1000.0 / fps_ : 0.0); put(line);
        std::snprintf(line, sizeof line, "POS: %.2f, %.2f  ANGLE: %.2f", position_.x, position_.y, angle_); put(line);
        std::snprintf(line, sizeof line, "MAP: %dX%d  ORBS: %zu", level().map.width(), level().map.height(), orbs_.size());
        put(line);
        std::snprintf(line, sizeof line, "RENDER: %s  FOG: %s", textured_ ? "TEXTURED" : "FLAT",
                      textured_ ? upper(ge::opt::fog_kernel_name(ge::opt::active_fog_kernel())).c_str() : "OFF");
        put(line);
        put(ge::opt::asm_enabled() ? "ROUTINES: NASM X86-64" : "ROUTINES: C++ FALLBACK");
    }

    void draw_help(ge::Framebuffer& fb, bool paused) const {
        const char* lines[] = {"WASD / ARROWS: MOVE AND TURN", "MOUSE: LOOK   CLICK/SPACE: SHOOT", "E: NEXT LEVEL   R: TEXTURES",
                               "F3: STATISTICS   ESC: PAUSE", "F2: EDIT MODE (SHOOTING OFF)",
                               "LEFT CLICK: SELECT  RIGHT: INSERT", "WHEEL: CHOOSE WHAT TO INSERT",
                               "X/DEL: REMOVE   C: CHANGE TO CHOICE", "M: MOVE   , .: ROTATE PLAYER START",
                               "BACKSPACE: DESELECT", "F5: SAVE   F9: RELOAD FROM FILE"};
        const int count = paused ? 11 : 5;  // the start-up hint stays short; the pause panel lists everything
        const int w = std::min(fb.width() - 16, 232), h = 46 + count * 11 + (paused && editor_.dirty() ? 11 : 0);
        const int x = (fb.width() - w) / 2, y = (fb.height() - h) / 2;
        fill_rect(fb, x, y, w, h, 0xFF101820u);
        fill_rect(fb, x, y, w, 1, kYellow); fill_rect(fb, x, y + h - 1, w, 1, kYellow);
        fill_rect(fb, x, y, 1, h, kYellow); fill_rect(fb, x + w - 1, y, 1, h, kYellow);
        centered(fb, y + 8, paused ? "PAUSED" : "SORTED VIEWER", kYellow);
        int ly = y + 24;
        for (int i = 0; i < count; ++i) { centered(fb, ly, lines[i], kWhite); ly += 11; }
        if (paused && editor_.dirty()) centered(fb, ly, "UNSAVED EDITS: Q DISCARDS THEM", kRed);
        centered(fb, y + h - 14, paused ? "ESC: RESUME   Q: QUIT" : "ESC: PAUSE", kGrey);
    }

    ge::EngineConfig config_;
    std::vector<ge::Level> levels_;
    std::vector<std::filesystem::path> paths_;
    ge::LevelEditor editor_;
    std::uint64_t marker_revision_ = ~std::uint64_t{0};
    bool entered_ = false;  // editor_ holds a level
    bool edit_ = false;
    ge::CrosshairTarget target_;
    std::vector<ge::Sprite> sprites_;
    std::string message_;
    bool message_ok_ = true;
    double message_time_ = 0.0;
    std::size_t start_ = 0;
    std::size_t index_ = 0;
    bool textured_ = true;
    ge::TextureSet textures_;
    ge::Renderer renderer_;
    ge::Vec2 position_{1.5, 1.5};
    double angle_ = 0.0;
    std::deque<Orb> orbs_;
    std::vector<Marker> markers_;
    int hits_ = 0;
    double cooldown_ = 0.0;
    double elapsed_ = 0.0;
    bool paused_ = false, debug_ = false, quit_ = false;
    Clock::time_point last_frame_;
    double fps_ = 60.0;
};

bool has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}
const char* arg_value(int argc, char** argv, const char* flag) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return nullptr;
}

void print_usage(std::FILE* out) {
    const char* options = std::strchr(ge::usage_text(), '\n');  // reuse the engine's option list, drop its first line
    std::fprintf(out,
                 "SORTED Viewer: walk around a level, shoot orbs, inspect the engine.\n"
                 "Usage: sorted-viewer [options]\n"
                 "Controls: WASD/arrows move, mouse look, click/Space shoot, E next level, R textures,\n"
                 "          F3 statistics, Esc pause, Q quit (while paused)\n"
                 "Editing:  F2 edit mode, left click select, right click insert, wheel or [ ] choose, X/Delete remove,\n"
                 "          M move, C change, , . rotate the player start, Backspace deselect, F5 save, F9 reload\n"
                 "Options:%s",
                 options ? options : "\n");
}

}  // namespace

int main(int argc, char** argv) {
    ge::EngineConfig config;
    config.width = 384;
    config.height = 240;
    config.max_frames = 600;       // headless default; a window runs until it is closed
    config.deterministic = true;

    {
        const char* explicit_path = arg_value(argc, argv, "--settings");
        const std::filesystem::path path = explicit_path ? explicit_path : "settings.ini";
        std::error_code ec;
        if (explicit_path != nullptr || std::filesystem::is_regular_file(path, ec)) {
            const ge::Status s = ge::apply_settings_file(path, config);
            if (!s) { std::fprintf(stderr, "error: %s\n", s.message.c_str()); return 64; }
        }
    }
    const ge::ParseResult parsed = ge::parse_arguments(argc, argv, config);
    if (!parsed.ok) { std::fprintf(stderr, "error: %s\n\n", parsed.error.c_str()); print_usage(stderr); return 64; }
    if (parsed.help_requested) { print_usage(stdout); return 0; }

    // ---- levels: every level in the assets folder (or the one named with --level) -------------------
    std::optional<ge::ResourceManager> resources;
    {
        std::optional<std::filesystem::path> assets = config.assets_dir.empty() ? ge::find_assets_dir(argv[0])
                                                                                 : std::optional<std::filesystem::path>(config.assets_dir);
        if (assets && ge::looks_like_assets_dir(*assets)) resources.emplace(*assets);
        else if (!config.assets_dir.empty()) {
            std::fprintf(stderr, "error: '%s' is not an assets directory (it has no 'levels' folder)\n", config.assets_dir.c_str());
            return 1;
        }
    }
    if (config.list_levels) {
        if (!resources) { std::fprintf(stderr, "error: assets directory not found (use --assets DIR or set GE_ASSETS)\n"); return 1; }
        for (const std::string& n : resources->list_levels()) std::printf("%s\n", n.c_str());
        return 0;
    }
    std::vector<ge::Level> levels;
    std::vector<std::filesystem::path> level_paths;  // the file behind each level, for saving
    std::size_t start_index = 0;
    bool level_found = false;
    if (resources) {
        for (const std::string& name : resources->list_levels()) {
            std::string ignored;
            if (auto level = resources->load_level(name, ignored)) {
                if (name == config.level) { start_index = levels.size(); level_found = true; }  // --level NAME
                levels.push_back(std::move(*level));
                level_paths.push_back(resources->root() / "levels" / (name + ".txt"));
            }
        }
    }
    if (!config.level.empty() && !level_found) {  // --level FILE: load that file and start on it
        std::string error;
        std::optional<ge::Level> chosen = resources ? resources->load_level(config.level, error) : ge::load_level_file(config.level, error);
        if (!chosen) { std::fprintf(stderr, "error: %s\n", error.c_str()); return 1; }
        levels.insert(levels.begin(), std::move(*chosen));
        level_paths.insert(level_paths.begin(), std::filesystem::path(config.level));
        start_index = 0;
    }
    if (levels.empty()) {
        std::fprintf(stderr, "[viewer] no assets found (use --assets DIR); showing the built-in test level\n");
        levels.push_back(ge::test_level());
        level_paths.emplace_back();  // built in: nothing to save to
    }

    // ---- window or headless ---------------------------------------------------------------------------
    bool window = false;
    if (config.mode == ge::RunMode::Window) window = true;
    else if (config.mode == ge::RunMode::Auto) window = ge::window_platform_compiled() && ge::display_available();

    std::unique_ptr<ge::IPlatform> platform;
    if (window) {
        platform = ge::make_window_platform();
        if (!platform) { std::fprintf(stderr, "error: this build has no window support (needs SDL3)\n"); return 1; }
        if (!has_flag(argc, argv, "--frames")) config.max_frames = 0;
        if (!has_flag(argc, argv, "--deterministic")) config.deterministic = false;
    } else {
        if (config.max_frames == 0) { std::fprintf(stderr, "error: the headless platform has no window to close, so --frames must be > 0\n"); return 64; }
        std::printf("[viewer] no window (%s); running a short scripted walk (use --window to force)\n",
                    ge::window_platform_compiled() ? "no display found" : "built without SDL3");
        // walk, turn, shoot: enough to exercise movement, collision, sprites and orbs without a window
        platform = std::make_unique<ge::HeadlessPlatform>(ge::ScriptedInput(std::vector<ge::InputSegment>{
            {90, 1.0, 0.0, 0.0, true}, {40, 0.0, 0.0, 0.5, true}, {90, 1.0, 0.0, 0.0, true}, {40, 0.0, 1.0, -0.3, true}}));
    }

    ge::Engine engine(config, std::move(platform));
    if (const ge::Status started = engine.initialize(); !started) {
        std::fprintf(stderr, "error: %s\n", started.message.c_str());
        return 1;
    }
    Viewer viewer(std::move(levels), std::move(level_paths), start_index, config.textured);
    const int rc = engine.run(viewer);
    if (rc != 0) return rc;
    if (!window) {
        const ge::EngineStats& stats = engine.stats();
        std::printf("SORTED Viewer: %llu frames, %.2f simulated seconds, checksum %016llx\n",
                    static_cast<unsigned long long>(stats.frames), stats.simulated_seconds,
                    static_cast<unsigned long long>(engine.framebuffer().checksum()));
    }
    return 0;
}
