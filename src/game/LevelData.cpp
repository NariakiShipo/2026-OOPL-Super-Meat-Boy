#include "game/LevelData.hpp"

namespace Game {
std::vector<LevelConfig> BuildDefaultLevels() {
    using Obj = LevelObjectConfig;

    LevelConfig level1;
    level1.spawn = {-520.0F, -40.0F};
    level1.goalPosition = {470.0F, 120.0F};
    level1.goalSize = {140.0F, 140.0F};
    level1.goalTexturePath = "images/bandagegirl.png";
    level1.platforms = {
        Obj{{0.0F, -320.0F}, {1300.0F, 70.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-220.0F, -120.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{220.0F, 20.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{-800.0F, -120.0F}, {280.0F, 500.0F}, 2.0F, "images/sawshooter.png"},
    };
    level1.deathZones = {
        Obj{{-60.0F, -240.0F}, {180.0F, 40.0F}, 3.0F, "images/monsterts.png"},
        Obj{{360.0F, -260.0F}, {180.0F, 40.0F}, 3.0F, "images/monsterts.png"},
    };

    LevelConfig level2;
    level2.spawn = {-580.0F, -220.0F};
    level2.goalPosition = {560.0F, 240.0F};
    level2.goalSize = {140.0F, 140.0F};
    level2.goalTexturePath = "images/bandagegirl.png";
    level2.platforms = {
        Obj{{0.0F, -340.0F}, {1400.0F, 60.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-350.0F, -120.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{-80.0F, 20.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{180.0F, 140.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{430.0F, 240.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
    };
    level2.deathZones = {
        Obj{{-210.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
        Obj{{70.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
        Obj{{340.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
    };

    return {level1, level2};
}
} // namespace Game
