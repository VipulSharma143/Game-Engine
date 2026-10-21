#include "game/DemoTour.hpp"

namespace game {

std::vector<ge::InputSegment> demo_tour() {
    return {
        {60, 1.0, 0.0, 0.0, true},   // walk east, firing down the corridor
        {45, 0.0, 0.0, 0.5, true},   // turn right, firing
        {60, 1.0, 0.0, 0.0, true},   // walk
        {30, 1.0, 0.0, 0.5, true},   // curve right
        {60, 1.0, 0.0, 0.0, true},   // walk, will meet a wall and slide
        {45, 0.0, 1.0, 0.0, true},   // strafe right
    };
}

}  // namespace game
