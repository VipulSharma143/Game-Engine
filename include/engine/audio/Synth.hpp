#pragma once
// Procedurally generated sound effects: no audio files, identical output on every platform.

#include "engine/audio/Sound.hpp"

namespace ge {

struct SoundBank {
    Sound shot;        // gunshot: noise burst over a falling square tone
    Sound hit;         // short high blip
    Sound kill;        // rising three-note arpeggio
    Sound reload;      // two mechanical clicks
    Sound wave_start;  // two rising tones
    Sound wave_clear;  // four-note upward arpeggio
    Sound win;         // fanfare
    Sound ui;          // menu blip
    Sound hurt;        // player takes damage
    Sound die;         // player dies
    Sound pickup;      // collect an item
    Sound door;        // door opens
    Sound locked;      // door is locked

    static SoundBank make(int sample_rate = 44100);
};

}  // namespace ge
