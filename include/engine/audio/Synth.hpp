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
    Sound shotgun;     // heavy low boom with a long noise tail
    Sound chaingun;    // short sharp crack, played many times a second
    Sound rocket;      // launch whoosh: noise over a rising-then-falling tone
    Sound explosion;   // long rumble with a hard noise burst at the start
    Sound enemy_shot;  // an enemy's bolt: a falling electric zap
    Sound switch_weapon;  // two quick mechanical clicks

    static SoundBank make(int sample_rate = 44100);
};

}  // namespace ge
