#include "game/LevelData.hpp"

#include "game/TmxLoader.hpp"

#include "Util/Logger.hpp"

namespace Game {
std::vector<LevelConfig> BuildDefaultLevels() {
    std::vector<LevelConfig> levels;
    levels.reserve(2);

    try {
        levels.push_back(LoadLevelFromTmx("images/forest1.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest2.tmx"));
        return levels;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to build TMX levels: {}", e.what());
    }

    using Obj = LevelObjectConfig;

    LevelConfig fallback1;
    fallback1.spawn = {-520.0F, -40.0F};
    fallback1.goalPosition = {470.0F, 120.0F};
    fallback1.goalSize = {30.0F, 30.0F};
    fallback1.goalTexturePath = "images/bandagegirl.png";
    fallback1.platforms = {
        Obj{{0.0F, -320.0F}, {1300.0F, 70.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-220.0F, -120.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{220.0F, 20.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{-800.0F, -120.0F}, {280.0F, 500.0F}, 2.0F, "images/sawshooter.png"},
    };

    LevelConfig fallback2;
    fallback2.spawn = {-580.0F, -220.0F};
    fallback2.goalPosition = {560.0F, 240.0F};
    fallback2.goalSize = {30.0F, 30.0F};
    fallback2.goalTexturePath = "images/bandagegirl.png";
    fallback2.platforms = {
        Obj{{0.0F, -340.0F}, {1400.0F, 60.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-350.0F, -120.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{-80.0F, 20.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{180.0F, 140.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{430.0F, 240.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
    };

    return {fallback1, fallback2};
}
} // namespace Game
