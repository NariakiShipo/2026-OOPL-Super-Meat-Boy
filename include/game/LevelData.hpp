#ifndef GAME_LEVEL_DATA_HPP
#define GAME_LEVEL_DATA_HPP

#include "pch.hpp"

namespace Game {
struct LevelObjectConfig {
    glm::vec2 position;
    glm::vec2 size;
    float zIndex;
    std::string texturePath;
};

struct LevelConfig {
    glm::vec2 spawn;
    glm::vec2 goalPosition;
    glm::vec2 goalSize;
    std::string goalTexturePath;
    std::vector<LevelObjectConfig> platforms;
    std::vector<LevelObjectConfig> deathZones;
};

std::vector<LevelConfig> BuildDefaultLevels();
} // namespace Game

#endif
