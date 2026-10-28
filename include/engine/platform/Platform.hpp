#pragma once

#include <cstdint>

#include "engine/core/Config.hpp"
#include "engine/core/Status.hpp"
#include "engine/renderer/Framebuffer.hpp"

namespace ge {

// Device-independent input sampled once per frame. Axes are in [-1, 1].
struct InputState {
    double forward = 0.0;  // +1 = forward
    double strafe = 0.0;   // +1 = right
    double turn = 0.0;     // +1 = turn right (clockwise on the map)
    double look = 0.0;     // horizontal mouse motion this frame, in pixels (+ = right)
    bool fire = false;     // held
    bool use = false;      // pressed this frame (edge)
    bool reload = false;   // pressed this frame (edge)
    bool toggle_debug = false;  // pressed this frame (edge)
    bool pause = false;         // pressed this frame (edge)
    bool confirm = false;       // pressed this frame (edge): menus
    bool menu_quit = false;     // pressed this frame (edge): quit from a menu
    bool quit = false;

    // Level editing (SORTED Viewer). Edges like `use`. The SDL platform maps them to fixed keys and mouse
    // buttons (they are not in settings.ini); see docs/USING_SORTED.md.
    bool edit_toggle = false;    // F2: edit mode on/off
    bool edit_select = false;    // left click
    bool edit_insert = false;    // right click
    bool edit_remove = false;    // Delete or X
    bool edit_move = false;      // M
    bool edit_change = false;    // C
    bool edit_deselect = false;  // Backspace
    bool edit_save = false;      // F5
    bool edit_load = false;      // F9
    int edit_palette = 0;        // mouse wheel or ] / [ : steps this frame
    int edit_rotate = 0;         // . / , : steps this frame

    // Clears everything that must act once: presses and per-frame deltas (held keys and axes stay).
    void clear_edges() noexcept {
        look = 0.0;
        use = reload = toggle_debug = pause = confirm = menu_quit = false;
        edit_toggle = edit_select = edit_insert = edit_remove = edit_move = false;
        edit_change = edit_deselect = edit_save = edit_load = false;
        edit_palette = edit_rotate = 0;
    }
    // Adds the presses and deltas of `other` to this state (used to carry them over to the next frame).
    void add_edges(const InputState& other) noexcept {
        look += other.look;
        use |= other.use; reload |= other.reload; toggle_debug |= other.toggle_debug;
        pause |= other.pause; confirm |= other.confirm; menu_quit |= other.menu_quit;
        edit_toggle |= other.edit_toggle; edit_select |= other.edit_select; edit_insert |= other.edit_insert;
        edit_remove |= other.edit_remove; edit_move |= other.edit_move; edit_change |= other.edit_change;
        edit_deselect |= other.edit_deselect; edit_save |= other.edit_save; edit_load |= other.edit_load;
        edit_palette += other.edit_palette;
        edit_rotate += other.edit_rotate;
    }
};

// Everything operating-system specific (window, input devices, presentation)
// sits behind this interface. The engine never includes OS headers.
class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual Status init(const EngineConfig& config) = 0;
    virtual void shutdown() noexcept = 0;
    virtual void poll_input(InputState& input) = 0;
    virtual void present(const Framebuffer& frame, std::uint64_t frame_index) = 0;
    // Called once after the last frame; headless output (PPM, ASCII) is written here.
    virtual void finalize(const Framebuffer& final_frame) = 0;
    // The application asks for the mouse to be captured (gameplay) or free (menus). Default: ignore.
    virtual void set_mouse_capture(bool) {}
};

}  // namespace ge
