#ifndef GAME_LEVEL_DATA_HPP
#define GAME_LEVEL_DATA_HPP

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <vector>

namespace Game {

enum class WorldCategory { Forest, Factory };

enum class ShootDirection { Up, Down, Left, Right };

struct ShooterConfig {
    glm::vec2 position;
    ShootDirection direction = ShootDirection::Right;
};

struct WorldInfo {
    WorldCategory category = WorldCategory::Forest;
    std::string name;           // "Forest" / "Factory"
    std::string bgImagePath;    // 關卡選擇背景圖路徑
    int levelIndexInWorld = 0;  // 此關在本 world 的第幾關（0-based）
};

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

// 旋轉鋸：鋸片繞軸心旋轉
// dual=true  → 雙鋸對轉（旋臂貫穿軸心、兩端各一鋸）
// dual=false → 單鋸繞軸（旋臂從軸心伸出、末端一鋸）
struct RotorConfig {
    glm::vec2 pivot = {0.0F, 0.0F};  // 軸心（world 座標）
    float armRadius = 70.0F;         // 軸心到鋸片中心距離
    float sawDiameter = 70.0F;       // 鋸片直徑
    bool dual = true;
    float startAngleDeg = 0.0F;      // 起始角（CCW；0=旋臂朝右），來自 TMX 物件 rotation
};

// Boss 關卡參數（W1 Lil' Slugger 追擊戰）
struct BossLevelConfig {
    bool enabled = false;
    glm::vec2 spawn = {0.0F, 0.0F};  // Boss 起點（玩家後方）
    float rushTriggerX = 0.0F;       // 玩家越過此 x → Boss 加速衝撞
    float crashX = 0.0F;             // Boss 抵達此 x → 撞牆停止
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
    std::vector<ShooterConfig> shooters;
    std::vector<RotorConfig> rotors;
    WorldInfo worldInfo;
    BossLevelConfig boss;
};

struct WorldData {
    WorldCategory category;
    std::string name;
    std::string bgImagePath;
    std::vector<LevelConfig> levels;
};

std::vector<WorldData> BuildWorldData();
std::vector<LevelConfig> BuildDefaultLevels();
LevelConfig BuildBossTestLevel();  // 步驟1：S0–S1 手刻測試地形（之後改 forestboss.tmx）
} // namespace Game

#endif
