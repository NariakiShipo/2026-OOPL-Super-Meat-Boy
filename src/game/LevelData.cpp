#include "game/LevelData.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "common/AssetPath.hpp"
#include "game/TmxLoader.hpp"

#include "Util/Logger.hpp"

namespace {
using json = nlohmann::json;

constexpr const char *kLevelsConfigPath = "config/levels.json";

bool ReadJsonConfig(const std::string &relativePath, json *outJson) {
    if (outJson == nullptr) {
        return false;
    }

    const std::string resolvedPath = Common::ResolveAssetPath(relativePath);
    std::ifstream input(resolvedPath);
    if (!input.is_open()) {
        return false;
    }

    try {
        input >> *outJson;
    } catch (const std::exception &e) {
        LOG_WARN("Failed to parse JSON config '{}': {}", resolvedPath, e.what());
        return false;
    }

    return true;
}

glm::vec2 ParseVec2(const json &value, const glm::vec2 &fallback) {
    if (!value.is_array() || value.size() < 2) {
        return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

glm::vec4 ParseVec4(const json &value, const glm::vec4 &fallback) {
    if (!value.is_array() || value.size() < 4) {
        return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>(),
            value[2].get<float>(), value[3].get<float>()};
}

Game::LevelObjectConfig ParseLevelObject(const json &value) {
    Game::LevelObjectConfig cfg{};
    if (!value.is_object()) {
        return cfg;
    }

    cfg.position = ParseVec2(value.value("position", json::array()), cfg.position);
    cfg.size = ParseVec2(value.value("size", json::array()), cfg.size);
    cfg.zIndex = value.value("zIndex", cfg.zIndex);
    cfg.texturePath = value.value("texturePath", cfg.texturePath);
    cfg.uvRect = ParseVec4(value.value("uvRect", json::array()), cfg.uvRect);
    cfg.visible = value.value("visible", cfg.visible);
    cfg.animationIntervalMs =
        value.value("animationIntervalMs", cfg.animationIntervalMs);

    if (value.contains("animationFrames") && value["animationFrames"].is_array()) {
        cfg.animationFrames.clear();
        for (const auto &frame : value["animationFrames"]) {
            if (frame.is_string()) {
                cfg.animationFrames.push_back(frame.get<std::string>());
            }
        }
    }

    return cfg;
}

Game::LevelConfig ParseLevelConfig(const json &value) {
    Game::LevelConfig cfg{};
    if (!value.is_object()) {
        return cfg;
    }

    cfg.mapPath = value.value("mapPath", cfg.mapPath);
    cfg.mapPixelSize = ParseVec2(value.value("mapPixelSize", json::array()), cfg.mapPixelSize);
    cfg.worldBoundsMin =
        ParseVec2(value.value("worldBoundsMin", json::array()), cfg.worldBoundsMin);
    cfg.worldBoundsMax =
        ParseVec2(value.value("worldBoundsMax", json::array()), cfg.worldBoundsMax);
    cfg.spawn = ParseVec2(value.value("spawn", json::array()), cfg.spawn);
    cfg.goalPosition =
        ParseVec2(value.value("goalPosition", json::array()), cfg.goalPosition);
    cfg.goalSize = ParseVec2(value.value("goalSize", json::array()), cfg.goalSize);
    cfg.goalTexturePath = value.value("goalTexturePath", cfg.goalTexturePath);

    const auto parseObjectList = [](const json &list,
                                    std::vector<Game::LevelObjectConfig> *out) {
        if (out == nullptr || !list.is_array()) {
            return;
        }
        out->clear();
        for (const auto &entry : list) {
            out->push_back(ParseLevelObject(entry));
        }
    };

    parseObjectList(value.value("renderTiles", json::array()), &cfg.renderTiles);
    parseObjectList(value.value("platforms", json::array()), &cfg.platforms);
    parseObjectList(value.value("breakableBlocks", json::array()), &cfg.breakableBlocks);
    parseObjectList(value.value("deathZones", json::array()), &cfg.deathZones);

    return cfg;
}

Game::WorldCategory ParseWorldCategory(const std::string &name) {
    if (name == "Factory") {
        return Game::WorldCategory::Factory;
    }
    return Game::WorldCategory::Forest;
}

std::vector<Game::LevelConfig> ParseFallbackLevels(const json &root) {
    std::vector<Game::LevelConfig> levels;
    if (!root.contains("fallbackLevels") || !root["fallbackLevels"].is_array()) {
        return levels;
    }

    for (const auto &entry : root["fallbackLevels"]) {
        levels.push_back(ParseLevelConfig(entry));
    }

    return levels;
}

std::vector<std::string> ParseTmxLevels(const json &root) {
    std::vector<std::string> paths;
    if (!root.contains("tmxLevels") || !root["tmxLevels"].is_array()) {
        return paths;
    }

    for (const auto &entry : root["tmxLevels"]) {
        if (entry.is_string()) {
            paths.push_back(entry.get<std::string>());
        }
    }
    return paths;
}
} // namespace

namespace Game {

std::vector<WorldData> BuildWorldData() {
    std::vector<WorldData> worlds;

    json configRoot;
    if (!ReadJsonConfig(kLevelsConfigPath, &configRoot)) {
        // 無 config：回退到預設 forest 世界
        WorldData forestWorld;
        forestWorld.category = WorldCategory::Forest;
        forestWorld.name = "Forest";
        forestWorld.bgImagePath = "images/forestlevelselect.png";

        try {
            const std::vector<std::string> forestPaths = {
                "images/forest1.tmx", "images/forest2.tmx",
                "images/forest3.tmx", "images/forest4.tmx",
                "images/forest5.tmx", "images/forest6.tmx",
                "images/forest7.tmx", "images/forest8.tmx",
            };
            for (int i = 0; i < static_cast<int>(forestPaths.size()); ++i) {
                auto level = LoadLevelFromTmx(forestPaths[i]);
                level.worldInfo = {WorldCategory::Forest, "Forest",
                                   forestWorld.bgImagePath, i};
                forestWorld.levels.push_back(std::move(level));
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to build default forest levels: {}", e.what());
        }

        if (!forestWorld.levels.empty()) {
            worlds.push_back(std::move(forestWorld));
        }
        return worlds;
    }

    // 解析新格式：worlds 陣列
    if (configRoot.contains("worlds") && configRoot["worlds"].is_array()) {
        for (const auto &worldJson : configRoot["worlds"]) {
            if (!worldJson.is_object()) {
                continue;
            }

            WorldData world;
            world.name = worldJson.value("name", std::string("Unknown"));
            world.bgImagePath = worldJson.value("bgImage", std::string(""));
            world.category = ParseWorldCategory(world.name);

            const auto tmxPaths = ParseTmxLevels(worldJson);
            bool loadOk = true;
            try {
                for (int i = 0; i < static_cast<int>(tmxPaths.size()); ++i) {
                    auto level = LoadLevelFromTmx(tmxPaths[i]);
                    level.worldInfo = {world.category, world.name,
                                       world.bgImagePath, i};
                    world.levels.push_back(std::move(level));
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to load TMX for world '{}': {}", world.name, e.what());
                loadOk = false;
            }

            if (loadOk && !world.levels.empty()) {
                worlds.push_back(std::move(world));
            }
        }

        if (!worlds.empty()) {
            return worlds;
        }
    }

    // 舊格式回退：直接讀 tmxLevels（向後相容）
    const auto tmxPaths = ParseTmxLevels(configRoot);
    if (!tmxPaths.empty()) {
        WorldData forestWorld;
        forestWorld.category = WorldCategory::Forest;
        forestWorld.name = "Forest";
        forestWorld.bgImagePath = "images/forestlevelselect.png";

        try {
            for (int i = 0; i < static_cast<int>(tmxPaths.size()); ++i) {
                auto level = LoadLevelFromTmx(tmxPaths[i]);
                level.worldInfo = {WorldCategory::Forest, "Forest",
                                   forestWorld.bgImagePath, i};
                forestWorld.levels.push_back(std::move(level));
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to build TMX levels: {}", e.what());
        }

        if (!forestWorld.levels.empty()) {
            worlds.push_back(std::move(forestWorld));
            return worlds;
        }
    }

    // 最終回退：hardcoded fallback levels
    const auto fallbackLevels = ParseFallbackLevels(configRoot);
    if (!fallbackLevels.empty()) {
        WorldData fallbackWorld;
        fallbackWorld.category = WorldCategory::Forest;
        fallbackWorld.name = "Forest";
        fallbackWorld.bgImagePath = "images/forestlevelselect.png";
        fallbackWorld.levels = fallbackLevels;
        worlds.push_back(std::move(fallbackWorld));
    }

    return worlds;
}

std::vector<LevelConfig> BuildDefaultLevels() {
    const auto worlds = BuildWorldData();
    std::vector<LevelConfig> all;
    for (const auto &world : worlds) {
        for (const auto &level : world.levels) {
            all.push_back(level);
        }
    }
    if (!all.empty()) {
        return all;
    }

    // 最終 hardcoded fallback
    std::vector<LevelConfig> levels;
    try {
        levels.push_back(LoadLevelFromTmx("images/forest1.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest2.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest3.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest4.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest5.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest6.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest7.tmx"));
        levels.push_back(LoadLevelFromTmx("images/forest8.tmx"));
        return levels;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to build TMX levels: {}", e.what());
    }

    using Obj = LevelObjectConfig;

    LevelConfig fallback1;
    fallback1.mapPixelSize = {1400.0F, 900.0F};
    fallback1.worldBoundsMin = {-700.0F, -450.0F};
    fallback1.worldBoundsMax = {700.0F, 450.0F};
    fallback1.spawn = {-520.0F, -40.0F};
    fallback1.goalPosition = {470.0F, 120.0F};
    fallback1.goalSize = {30.0F, 30.0F};
    fallback1.goalTexturePath = "images/bandagegirl.png";
    fallback1.platforms = {
        Obj{{0.0F, -320.0F}, {1300.0F, 70.0F}, 1.0F, "images/disappearing.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{-220.0F, -120.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{220.0F, 20.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{-800.0F, -120.0F}, {280.0F, 500.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
    };

    return {fallback1};
}

// ── W1 Boss「Lil' Slugger」步驟1 測試關 ──────────────────────────
// S0–S1 短版手刻地形：平地 + 鋸片×3 + 跳坑 + 終點高台。
// 之後步驟3 改為 forestboss.tmx 完整 S0–S8 地形。
//
// 佈局（x 由左到右）：
//   -2850  Boss 起點          -2400 玩家出生
//   -1500/-800/-100 地面鋸片   900..1080 跳坑
//   1700  (預留)               2400 高台起點 = rushTriggerX
//   2150  Boss 撞牆點          2750 Bandage Girl
LevelConfig BuildBossTestLevel() {
    using Obj = LevelObjectConfig;

    LevelConfig cfg;
    cfg.mapPath = "boss_lilslugger_test";
    cfg.mapPixelSize = {6000.0F, 900.0F};
    cfg.worldBoundsMin = {-3000.0F, -450.0F};
    cfg.worldBoundsMax = {3000.0F, 450.0F};
    cfg.spawn = {-2400.0F, -200.0F};
    cfg.goalPosition = {2750.0F, -140.0F};
    cfg.goalSize = {60.0F, 90.0F};
    cfg.goalTexturePath = "images/bandagegirl.png";
    cfg.worldInfo = {WorldCategory::Forest, "Forest",
                     "images/forestlevelselect.png", 8};

    // 背景（拉伸暫代）
    cfg.renderTiles = {
        Obj{{0.0F, 0.0F}, {6000.0F, 900.0F}, -50.0F, "images/forestbg.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
    };

    // 地面：跳坑(900..1080)切成左右兩段；右側終點高台
    cfg.platforms = {
        // 左段地面 -3000..900，頂面 y=-310
        Obj{{-1050.0F, -380.0F}, {3900.0F, 140.0F}, 1.0F,
            "images/foresttiles01.png", {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        // 右段地面 1080..3000
        Obj{{2040.0F, -380.0F}, {1920.0F, 140.0F}, 1.0F,
            "images/foresttiles01.png", {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        // 終點高台 2400..3000，頂面 y=-230（Boss 撞這面牆下方）
        Obj{{2700.0F, -270.0F}, {600.0F, 80.0F}, 2.0F,
            "images/factorytiles01.png", {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
    };

    // 地面鋸片×3（死亡區、循環動畫），半埋於地面、跳過即可
    const std::vector<std::string> sawFrames = {
        "images/buzzsaw2_1.png",
        "images/buzzsaw2_2.png",
        "images/buzzsaw2_3.png",
    };
    const auto makeSaw = [&sawFrames](const float x) {
        return Obj{{x, -265.0F}, {90.0F, 90.0F}, 5.0F, "",
                   {0.0F, 0.0F, 1.0F, 1.0F}, true, sawFrames, 80};
    };
    cfg.deathZones = {
        makeSaw(-1500.0F),
        makeSaw(-800.0F),
        makeSaw(-100.0F),
    };

    // Boss：起點在玩家後方，視覺底邊貼地（-210 - 100 = -310 = 地面頂，視覺 340x200）
    cfg.boss.enabled = true;
    cfg.boss.spawn = {-2850.0F, -210.0F};
    cfg.boss.rushTriggerX = 2400.0F;  // 玩家踏上高台 → Boss 加速
    cfg.boss.crashX = 2150.0F;        // 撞牆點（高台左牆前）

    return cfg;
}
} // namespace Game
