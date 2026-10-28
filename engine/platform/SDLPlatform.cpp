#include "engine/platform/SDLPlatform.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "engine/core/Logger.hpp"
#include "engine/platform/FrameDump.hpp"

namespace ge {

namespace {
using ScancodeList = std::vector<SDL_Scancode>;

// Resolves "W,Up" to scancodes. Returns an error message for the first unknown key name.
std::string resolve_keys(const std::string& action, const std::string& list, ScancodeList& out) {
    out.clear();
    const std::vector<std::string> names = split_key_list(list);
    if (names.empty()) return "binding '" + action + "' has no keys";
    for (const std::string& n : names) {
        const SDL_Scancode sc = SDL_GetScancodeFromName(n.c_str());
        if (sc == SDL_SCANCODE_UNKNOWN) return "unknown key '" + n + "' in binding '" + action + "' (use SDL key names such as W, Space, Left, Escape, Return, F3)";
        out.push_back(sc);
    }
    return {};
}

bool any_down(const bool* keys, const ScancodeList& list) {
    for (SDL_Scancode sc : list)
        if (keys[sc]) return true;
    return false;
}

bool contains(const ScancodeList& list, SDL_Scancode sc) {
    for (SDL_Scancode k : list)
        if (k == sc) return true;
    return false;
}
}  // namespace

struct SDLPlatform::Impl {
    ScancodeList forward, backward, left, right, turn_left, turn_right, fire, reload, use, debug, pause, confirm, quit_menu;
    bool capture_allowed = true;   // from settings / --no-mouse
    bool user_released = false;    // Tab toggled the mouse free
    bool wanted_capture = true;    // what the application asked for
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool mouse_captured = false;
    bool sdl_started = false;
    float wheel_accumulator = 0.0f;  // fractional wheel/touchpad scroll not yet worth a palette step
    std::string ppm_path;
    Uint64 title_ticks = 0;
    std::uint64_t title_frames = 0;
};

SDLPlatform::SDLPlatform() : impl_(std::make_unique<Impl>()) {}
SDLPlatform::~SDLPlatform() { shutdown(); }

Status SDLPlatform::init(const EngineConfig& config) {
    Impl& s = *impl_;
    {
        const KeyBindings& k = config.keys;
        const std::pair<const char*, std::pair<const std::string*, ScancodeList*>> table[] = {
            {"forward", {&k.forward, &s.forward}}, {"backward", {&k.backward, &s.backward}},
            {"left", {&k.left, &s.left}}, {"right", {&k.right, &s.right}},
            {"turn_left", {&k.turn_left, &s.turn_left}}, {"turn_right", {&k.turn_right, &s.turn_right}},
            {"fire", {&k.fire, &s.fire}}, {"reload", {&k.reload, &s.reload}}, {"use", {&k.use, &s.use}},
            {"debug", {&k.debug, &s.debug}}, {"pause", {&k.pause, &s.pause}},
            {"confirm", {&k.confirm, &s.confirm}}, {"quit_menu", {&k.quit_menu, &s.quit_menu}},
        };
        for (const auto& row : table) {
            const std::string problem = resolve_keys(row.first, *row.second.first, *row.second.second);
            if (!problem.empty()) return Status::failure(problem);
        }
        s.capture_allowed = config.capture_mouse;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) return Status::failure(std::string("SDL_Init: ") + SDL_GetError());
    s.sdl_started = true;
    s.window = SDL_CreateWindow("Game Engine", config.width * config.window_scale, config.height * config.window_scale,
                                SDL_WINDOW_RESIZABLE);
    if (!s.window) return Status::failure(std::string("SDL_CreateWindow: ") + SDL_GetError());
    s.renderer = SDL_CreateRenderer(s.window, nullptr);
    if (!s.renderer) return Status::failure(std::string("SDL_CreateRenderer: ") + SDL_GetError());
    SDL_SetRenderVSync(s.renderer, 1);
    SDL_SetRenderLogicalPresentation(s.renderer, config.width, config.height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    // Framebuffer pixels are 0xAARRGGBB, which is ARGB8888 on every platform.
    s.texture = SDL_CreateTexture(s.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, config.width,
                                  config.height);
    if (!s.texture) return Status::failure(std::string("SDL_CreateTexture: ") + SDL_GetError());
    SDL_SetTextureScaleMode(s.texture, SDL_SCALEMODE_NEAREST);
    if (config.capture_mouse) s.mouse_captured = SDL_SetWindowRelativeMouseMode(s.window, true);
    s.wanted_capture = true;
    s.ppm_path = config.headless.ppm_path;
    s.title_ticks = SDL_GetTicks();
    GE_LOG_INFO("SDL3 video driver: %s", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?");
    return Status::success();
}

void SDLPlatform::shutdown() noexcept {
    Impl& s = *impl_;
    if (s.texture) { SDL_DestroyTexture(s.texture); s.texture = nullptr; }
    if (s.renderer) { SDL_DestroyRenderer(s.renderer); s.renderer = nullptr; }
    if (s.window) { SDL_DestroyWindow(s.window); s.window = nullptr; }
    if (s.sdl_started) { SDL_Quit(); s.sdl_started = false; }
}

void SDLPlatform::set_mouse_capture(bool wanted) {
    Impl& s = *impl_;
    s.wanted_capture = wanted;
    if (!s.window || !s.capture_allowed) return;
    const bool should = wanted && !s.user_released;
    if (should != s.mouse_captured && SDL_SetWindowRelativeMouseMode(s.window, should)) s.mouse_captured = should;
}

void SDLPlatform::poll_input(InputState& input) {
    Impl& s = *impl_;
    double look = 0.0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: input.quit = true; break;
            case SDL_EVENT_KEY_DOWN: {
                if (e.key.repeat) break;
                const SDL_Scancode sc = e.key.scancode;
                if (contains(s.use, sc)) input.use = true;
                if (contains(s.reload, sc)) input.reload = true;
                if (contains(s.debug, sc)) input.toggle_debug = true;
                if (contains(s.pause, sc)) input.pause = true;
                if (contains(s.confirm, sc)) input.confirm = true;
                if (contains(s.quit_menu, sc)) input.menu_quit = true;
                // Editing keys are fixed (not in settings.ini): see InputState and docs/USING_SORTED.md.
                switch (sc) {
                    case SDL_SCANCODE_F2: input.edit_toggle = true; break;
                    case SDL_SCANCODE_DELETE: case SDL_SCANCODE_X: input.edit_remove = true; break;
                    case SDL_SCANCODE_M: input.edit_move = true; break;
                    case SDL_SCANCODE_C: input.edit_change = true; break;
                    case SDL_SCANCODE_BACKSPACE: input.edit_deselect = true; break;
                    case SDL_SCANCODE_F5: input.edit_save = true; break;
                    case SDL_SCANCODE_F9: input.edit_load = true; break;
                    case SDL_SCANCODE_RIGHTBRACKET: ++input.edit_palette; break;
                    case SDL_SCANCODE_LEFTBRACKET: --input.edit_palette; break;
                    case SDL_SCANCODE_PERIOD: ++input.edit_rotate; break;
                    case SDL_SCANCODE_COMMA: --input.edit_rotate; break;
                    default: break;
                }
                if (sc == SDL_SCANCODE_TAB && s.window && s.capture_allowed) {  // free / recapture the mouse
                    s.user_released = !s.user_released;
                    set_mouse_capture(s.wanted_capture);
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                input.confirm = true;
                if (e.button.button == SDL_BUTTON_LEFT) input.edit_select = true;
                if (e.button.button == SDL_BUTTON_RIGHT) input.edit_insert = true;
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                s.wheel_accumulator += e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
                const int steps = static_cast<int>(s.wheel_accumulator);  // whole notches only; touchpads send fractions
                input.edit_palette += steps;
                s.wheel_accumulator -= static_cast<float>(steps);
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: look += static_cast<double>(e.motion.xrel); break;
            default: break;
        }
    }
    input.look = s.mouse_captured ? look : 0.0;

    const bool* k = SDL_GetKeyboardState(nullptr);
    auto axis = [](bool plus, bool minus) { return (plus ? 1.0 : 0.0) - (minus ? 1.0 : 0.0); };
    input.forward = axis(any_down(k, s.forward), any_down(k, s.backward));
    input.strafe = axis(any_down(k, s.right), any_down(k, s.left));
    input.turn = axis(any_down(k, s.turn_right), any_down(k, s.turn_left));
    input.fire = any_down(k, s.fire) || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
}

void SDLPlatform::present(const Framebuffer& frame, std::uint64_t) {
    Impl& s = *impl_;
    SDL_UpdateTexture(s.texture, nullptr, frame.pixels().data(), frame.width() * static_cast<int>(sizeof(std::uint32_t)));
    SDL_RenderClear(s.renderer);
    SDL_RenderTexture(s.renderer, s.texture, nullptr, nullptr);
    SDL_RenderPresent(s.renderer);

    ++s.title_frames;
    const Uint64 now = SDL_GetTicks();
    if (now - s.title_ticks >= 1000) {
        char title[96];
        std::snprintf(title, sizeof title, "Game Engine - %.0f fps",
                      static_cast<double>(s.title_frames) * 1000.0 / static_cast<double>(now - s.title_ticks));
        SDL_SetWindowTitle(s.window, title);
        s.title_ticks = now;
        s.title_frames = 0;
    }
}

void SDLPlatform::finalize(const Framebuffer& final_frame) {
    if (!impl_->ppm_path.empty()) {
        const Status st = write_ppm(final_frame, impl_->ppm_path);
        if (st) GE_LOG_INFO("wrote %s", impl_->ppm_path.c_str());
        else GE_LOG_ERROR("%s", st.message.c_str());
    }
}

}  // namespace ge
