#include "game/DemoTour.hpp"

namespace game {

std::vector<ge::InputSegment> demo_tour() {
    return {
        {60, 1.0, 0.0, 0.0},   // walk east
        {45, 0.0, 0.0, 0.5},   // turn right
        {60, 1.0, 0.0, 0.0},   // walk
        {30, 1.0, 0.0, 0.5},   // curve right
        {60, 1.0, 0.0, 0.0},   // walk, will meet a wall and slide
        {45, 0.0, 1.0, 0.0},   // strafe right
    };
}

}  // namespace game
