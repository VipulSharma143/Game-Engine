// SORTED Viewer: the engine's own program. It opens a window, loads a level and lets you walk around it,
// shoot test orbs, switch levels and look at the engine's statistics. It uses only the public SORTED API
// (no code from game/), so it is also a worked example of a program built on the engine.
//
//   WASD / arrows  move and turn      mouse  look            click / Space  shoot an orb
//   E  next level                     R  textures on/off     F3  statistics
//   Esc  pause and help               Q  quit (while paused)
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
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/renderer/TextRenderer.hpp"
#include "engine/resources/AssetPath.hpp"
#include "engine/resources/ResourceManager.hpp"
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

constexpr std::uint32_t kWhite = 0xFFFFFFFFu;
constexpr std::uint32_t kYellow = 0xFFFFD84Au;
constexpr std::uint32_t kGrey = 0xFFB4BCC8u;

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

class Viewer final : public ge::Application {
public:
    Viewer(std::vector<ge::Level> levels, std::size_t start, bool textured)
        : levels_(std::move(levels)), start_(start), textured_(textured) {}

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
        if (in.use) enter_level((index_ + 1) % levels_.size());
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

        cooldown_ -= dt;
        if (in.fire && cooldown_ <= 0.0) {
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

        const ge::Camera camera = ge::Camera::from_angle(position_, angle_, config_.fov_degrees);
        renderer_.render(level().map, camera, fb);

        std::vector<ge::Sprite> sprites;
        for (const Marker& m : markers_) sprites.push_back({m.position, m.size, m.color, m.flash, m.diamond});
        for (const Orb& o : orbs_) sprites.push_back({o.position, o.stuck ? 0.10 : 0.14, o.stuck ? 0xFFB5531Cu : 0xFFFF9A3Cu, 0.0, false});
        ge::draw_sprites(camera, renderer_.depth(), sprites, fb);

        const int cx = fb.width() / 2, cy = fb.height() / 2;  // crosshair
        for (int d = 2; d <= 4; ++d) {
            fb.set_pixel(cx - d, cy, kWhite); fb.set_pixel(cx + d, cy, kWhite);
            fb.set_pixel(cx, cy - d, kWhite); fb.set_pixel(cx, cy + d, kWhite);
        }

        ge::draw_text_shadowed(fb, 4, 4, "SORTED VIEWER", kYellow);
        ge::draw_text_shadowed(fb, 4, 14, "LEVEL " + std::to_string(index_ + 1) + "/" + std::to_string(levels_.size()) + ": " + upper(level().name), kWhite);
        ge::draw_text_shadowed(fb, fb.width() - ge::text_width("ESC: HELP") - 4, 4, "ESC: HELP", kGrey);
        if (hits_ > 0) ge::draw_text_shadowed(fb, 4, 24, "HITS: " + std::to_string(hits_), kWhite);

        if (debug_) draw_stats(fb);
        if (paused_) draw_help(fb, true);
        else if (elapsed_ < kHelpSeconds) draw_help(fb, false);
    }

    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return quit_; }
    bool wants_mouse_capture() const noexcept override { return !paused_; }

private:
    using Clock = std::chrono::steady_clock;

    const ge::Level& level() const { return levels_[index_]; }

    static std::string upper(std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }

    void enter_level(std::size_t index) {
        index_ = index;
        position_ = level().spawn;
        angle_ = level().spawn_angle;
        orbs_.clear();
        hits_ = 0;
        markers_.clear();
        for (const ge::Vec2& p : level().target_spawns) markers_.push_back({p, 0xFF4FA3FFu, 0.45, false, true});
        for (const ge::Vec2& p : level().enemy_spawns) markers_.push_back({p, 0xFFE04040u, 0.55, false, true});
        for (const ge::PickupSpawn& p : level().pickups) {
            const std::uint32_t color = p.kind == ge::PickupKind::Health ? 0xFF4CD964u : p.kind == ge::PickupKind::Ammo ? 0xFFFFC107u : 0xFF40D0FFu;
            markers_.push_back({p.position, color, 0.30, true, false});
        }
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
        const int w = std::min(fb.width() - 16, 232), h = 100;
        const int x = (fb.width() - w) / 2, y = (fb.height() - h) / 2;
        fill_rect(fb, x, y, w, h, 0xFF101820u);
        fill_rect(fb, x, y, w, 1, kYellow); fill_rect(fb, x, y + h - 1, w, 1, kYellow);
        fill_rect(fb, x, y, 1, h, kYellow); fill_rect(fb, x + w - 1, y, 1, h, kYellow);
        centered(fb, y + 8, paused ? "PAUSED" : "SORTED VIEWER", kYellow);
        const char* lines[] = {"WASD / ARROWS: MOVE AND TURN", "MOUSE: LOOK", "CLICK OR SPACE: SHOOT ORBS",
                               "E: NEXT LEVEL   R: TEXTURES ON/OFF", "F3: STATISTICS   ESC: PAUSE"};
        int ly = y + 24;
        for (const char* line : lines) { centered(fb, ly, line, kWhite); ly += 11; }
        centered(fb, y + h - 14, paused ? "ESC: RESUME   Q: QUIT" : "ESC: PAUSE", kGrey);
    }

    ge::EngineConfig config_;
    std::vector<ge::Level> levels_;
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
    std::size_t start_index = 0;
    bool level_found = false;
    if (resources) {
        for (const std::string& name : resources->list_levels()) {
            std::string ignored;
            if (auto level = resources->load_level(name, ignored)) {
                if (name == config.level) { start_index = levels.size(); level_found = true; }  // --level NAME
                levels.push_back(std::move(*level));
            }
        }
    }
    if (!config.level.empty() && !level_found) {  // --level FILE: load that file and start on it
        std::string error;
        std::optional<ge::Level> chosen = resources ? resources->load_level(config.level, error) : ge::load_level_file(config.level, error);
        if (!chosen) { std::fprintf(stderr, "error: %s\n", error.c_str()); return 1; }
        levels.insert(levels.begin(), std::move(*chosen));
        start_index = 0;
    }
    if (levels.empty()) {
        std::fprintf(stderr, "[viewer] no assets found (use --assets DIR); showing the built-in test level\n");
        levels.push_back(ge::test_level());
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
    Viewer viewer(std::move(levels), start_index, config.textured);
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
