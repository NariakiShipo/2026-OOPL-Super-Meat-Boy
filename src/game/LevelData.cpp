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
std::vector<LevelConfig> BuildDefaultLevels() {
    std::vector<LevelConfig> levels;
    levels.reserve(8);

    json configRoot;
    if (ReadJsonConfig(kLevelsConfigPath, &configRoot)) {
        const auto tmxPaths = ParseTmxLevels(configRoot);
        if (!tmxPaths.empty()) {
            bool allLoaded = true;
            try {
                for (const auto &path : tmxPaths) {
                    levels.push_back(LoadLevelFromTmx(path));
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to build TMX levels from JSON: {}", e.what());
                allLoaded = false;
                levels.clear();
            }

            if (allLoaded && !levels.empty()) {
                return levels;
            }
        }

        const auto fallbackLevels = ParseFallbackLevels(configRoot);
        if (!fallbackLevels.empty()) {
            return fallbackLevels;
        }
    }

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

    LevelConfig fallback2;
    fallback2.mapPixelSize = {1600.0F, 1000.0F};
    fallback2.worldBoundsMin = {-800.0F, -500.0F};
    fallback2.worldBoundsMax = {800.0F, 500.0F};
    fallback2.spawn = {-580.0F, -220.0F};
    fallback2.goalPosition = {560.0F, 240.0F};
    fallback2.goalSize = {30.0F, 30.0F};
    fallback2.goalTexturePath = "images/bandagegirl.png";
    fallback2.platforms = {
        Obj{{0.0F, -340.0F}, {1400.0F, 60.0F}, 1.0F, "images/disappearing.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{-350.0F, -120.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{-80.0F, 20.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{180.0F, 140.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
        Obj{{430.0F, 240.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png",
            {0.0F, 0.0F, 1.0F, 1.0F}, true, {}, 0},
    };

    return {fallback1, fallback2};
}
} // namespace Game
