#ifndef GAME_LEVEL_DATA_HPP
#define GAME_LEVEL_DATA_HPP

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <vector>

namespace Game {
struct LevelObjectConfig {
    glm::vec2 position;
    glm::vec2 size;
    float zIndex;
    std::string texturePath;
    glm::vec4 uvRect = {0.0F, 0.0F, 1.0F, 1.0F};
    bool visible = true;
    std::vector<std::string> animationFrames;
    std::size_t animationIntervalMs = 0;
};

struct LevelConfig {
    std::string mapPath;
    glm::vec2 mapPixelSize = {0.0F, 0.0F};
    glm::vec2 worldBoundsMin = {0.0F, 0.0F};
    glm::vec2 worldBoundsMax = {0.0F, 0.0F};
    glm::vec2 spawn;
    glm::vec2 goalPosition;
    glm::vec2 goalSize;
    std::string goalTexturePath;
    std::vector<LevelObjectConfig> renderTiles;
    std::vector<LevelObjectConfig> platforms;
    std::vector<LevelObjectConfig> breakableBlocks;
    std::vector<LevelObjectConfig> deathZones;
};

std::vector<LevelConfig> BuildDefaultLevels();
} // namespace Game

#endif
