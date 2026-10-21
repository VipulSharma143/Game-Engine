#include "engine/platform/SDLPlatform.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

#include "engine/core/Logger.hpp"
#include "engine/platform/FrameDump.hpp"

namespace ge {

struct SDLPlatform::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool mouse_captured = false;
    bool sdl_started = false;
    std::string ppm_path;
    Uint64 title_ticks = 0;
    std::uint64_t title_frames = 0;
};

SDLPlatform::SDLPlatform() : impl_(std::make_unique<Impl>()) {}
SDLPlatform::~SDLPlatform() { shutdown(); }

Status SDLPlatform::init(const EngineConfig& config) {
    Impl& s = *impl_;
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

void SDLPlatform::poll_input(InputState& input) {
    Impl& s = *impl_;
    double look = 0.0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT: input.quit = true; break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.repeat) break;
                if (e.key.key == SDLK_ESCAPE) input.quit = true;
                else if (e.key.key == SDLK_E) input.use = true;
                else if (e.key.key == SDLK_R) input.reload = true;
                else if (e.key.key == SDLK_F3) input.toggle_debug = true;
                else if (e.key.key == SDLK_TAB && s.window) {  // release / recapture the mouse
                    s.mouse_captured = SDL_SetWindowRelativeMouseMode(s.window, !s.mouse_captured) ? !s.mouse_captured
                                                                                                    : s.mouse_captured;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION: look += static_cast<double>(e.motion.xrel); break;
            default: break;
        }
    }
    input.look = s.mouse_captured ? look : 0.0;

    const bool* k = SDL_GetKeyboardState(nullptr);
    auto axis = [&](bool plus, bool minus) { return (plus ? 1.0 : 0.0) - (minus ? 1.0 : 0.0); };
    input.forward = axis(k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP], k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]);
    input.strafe = axis(k[SDL_SCANCODE_D], k[SDL_SCANCODE_A]);
    input.turn = axis(k[SDL_SCANCODE_RIGHT], k[SDL_SCANCODE_LEFT]);
    input.fire = k[SDL_SCANCODE_SPACE] || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
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
