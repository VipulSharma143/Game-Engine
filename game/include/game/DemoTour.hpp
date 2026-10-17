#pragma once

#include <vector>

#include "engine/platform/ScriptedInput.hpp"

namespace game {

// Scripted walk through the test level used by the headless demo
// (about 5 seconds of simulation at 60 Hz).
std::vector<ge::InputSegment> demo_tour();

}  // namespace game
