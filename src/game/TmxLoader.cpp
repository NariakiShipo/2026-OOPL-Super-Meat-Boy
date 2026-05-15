#include "game/TmxLoader.hpp"

#include <fstream>
#include <filesystem>
#include <limits>

#include <nlohmann/json.hpp>

#include <tmxlite/Map.hpp>
#include <tmxlite/ImageLayer.hpp>
#include <tmxlite/Object.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/TileLayer.hpp>

#include "Util/Logger.hpp"
#include "common/AssetPath.hpp"

namespace {
using json = nlohmann::json;

constexpr const char *kTmxConfigPath = "config/tmx_mappings.json";

struct TmxAnimationConfig {
    std::vector<std::string> frames;
    std::size_t intervalMs = 0;
};

struct TmxConfig {
    std::string goalTexturePath = "images/bandagegirl.png";
    glm::vec2 goalDefaultSize = {96.0F, 96.0F};

    std::string stationaryLayerName = "stationary";
    std::string breakableLayerName = "breakable";
    std::string platformGroupName = "rectangle";
    std::string sawGroupName = "saws";
    std::string boundsGroupName = "bounds";

    std::uint32_t spawnLocalGid = 1;
    std::uint32_t goalLocalGid = 2;
    std::uint32_t disappearLocalGid = 3;
    std::uint32_t shooterLocalGidMin = 13;
    std::uint32_t shooterLocalGidMax = 16;

    float backgroundZ = -20.0F;
    float renderLayerZStep = 5.0F;

    TmxAnimationConfig sawAnimation{{
        "images/buzzsaw2_1.png",
        "images/buzzsaw2_2.png",
        "images/buzzsaw2_3.png",
    }, 80};

    TmxAnimationConfig breakableAnimation{{
        "images/disappearing_1.png",
        "images/disappearing_2.png",
        "images/disappearing_3.png",
        "images/disappearing_4.png",
        "images/disappearing_5.png",
        "images/disappearing_6.png",
        "images/disappearing_7.png",
        "images/disappearing_8.png",
        "images/disappearing_9.png",
        "images/disappearing_10.png",
        "images/disappearing_11.png",
    }, 70};

    glm::vec2 fallbackSpawnOffset = {80.0F, 80.0F};
    float boundaryWallThickness = 50.0F;
    float boundaryWallExtraHeight = 200.0F;
};

glm::vec2 ParseVec2(const json &value, const glm::vec2 &fallback) {
    if (!value.is_array() || value.size() < 2) {
        return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

std::vector<std::string> ParseStringList(const json &value,
                                         const std::vector<std::string> &fallback) {
    if (!value.is_array()) {
        return fallback;
    }

    std::vector<std::string> result;
    for (const auto &entry : value) {
        if (entry.is_string()) {
            result.push_back(entry.get<std::string>());
        }
    }
    return result.empty() ? fallback : result;
}

TmxAnimationConfig ParseAnimationConfig(const json &value,
                                        const TmxAnimationConfig &fallback) {
    if (!value.is_object()) {
        return fallback;
    }

    TmxAnimationConfig cfg = fallback;
    cfg.frames = ParseStringList(value.value("frames", json::array()), fallback.frames);
    cfg.intervalMs = value.value("intervalMs", fallback.intervalMs);
    return cfg;
}

TmxConfig ParseTmxConfig(const json &root) {
    TmxConfig cfg{};
    if (!root.is_object()) {
        return cfg;
    }

    if (root.contains("goal") && root["goal"].is_object()) {
        const auto &goal = root["goal"];
        cfg.goalTexturePath = goal.value("texturePath", cfg.goalTexturePath);
        cfg.goalDefaultSize =
            ParseVec2(goal.value("defaultSize", json::array()), cfg.goalDefaultSize);
    }

    cfg.stationaryLayerName = root.value("stationaryLayerName", cfg.stationaryLayerName);
    cfg.breakableLayerName = root.value("breakableLayerName", cfg.breakableLayerName);
    cfg.platformGroupName = root.value("platformGroupName", cfg.platformGroupName);
    cfg.sawGroupName = root.value("sawGroupName", cfg.sawGroupName);
    cfg.boundsGroupName = root.value("boundsGroupName", cfg.boundsGroupName);

    if (root.contains("tiles") && root["tiles"].is_object()) {
        const auto &tiles = root["tiles"];
        cfg.spawnLocalGid = tiles.value("spawnLocalGid", cfg.spawnLocalGid);
        cfg.goalLocalGid = tiles.value("goalLocalGid", cfg.goalLocalGid);
        cfg.disappearLocalGid = tiles.value("disappearLocalGid", cfg.disappearLocalGid);
        cfg.shooterLocalGidMin = tiles.value("shooterLocalGidMin", cfg.shooterLocalGidMin);
        cfg.shooterLocalGidMax = tiles.value("shooterLocalGidMax", cfg.shooterLocalGidMax);
    }

    if (root.contains("zIndex") && root["zIndex"].is_object()) {
        const auto &zIndex = root["zIndex"];
        cfg.backgroundZ = zIndex.value("background", cfg.backgroundZ);
        cfg.renderLayerZStep = zIndex.value("renderLayerStep", cfg.renderLayerZStep);
    }

    if (root.contains("animations") && root["animations"].is_object()) {
        const auto &animations = root["animations"];
        cfg.sawAnimation =
            ParseAnimationConfig(animations.value("saw", json::object()), cfg.sawAnimation);
        cfg.breakableAnimation = ParseAnimationConfig(
            animations.value("breakable", json::object()), cfg.breakableAnimation);
    }

    cfg.fallbackSpawnOffset =
        ParseVec2(root.value("fallbackSpawnOffset", json::array()), cfg.fallbackSpawnOffset);

    if (root.contains("boundaryWalls") && root["boundaryWalls"].is_object()) {
        const auto &walls = root["boundaryWalls"];
        cfg.boundaryWallThickness = walls.value("thickness", cfg.boundaryWallThickness);
        cfg.boundaryWallExtraHeight =
            walls.value("extraHeight", cfg.boundaryWallExtraHeight);
    }

    return cfg;
}

const TmxConfig &GetTmxConfig() {
    static TmxConfig config = []() {
        TmxConfig cfg{};
        const std::string resolvedPath = Common::ResolveAssetPath(kTmxConfigPath);
        std::ifstream input(resolvedPath);
        if (!input.is_open()) {
            return cfg;
        }

        try {
            json root;
            input >> root;
            cfg = ParseTmxConfig(root);
        } catch (const std::exception &e) {
            LOG_WARN("Failed to parse TMX config '{}': {}", resolvedPath, e.what());
        }

        return cfg;
    }();

    return config;
}

glm::vec2 TmxPixelCenterToWorld(const float centerX, const float centerY,
                                const float mapPixelWidth,
                                const float mapPixelHeight) {
    return {
        centerX - (mapPixelWidth * 0.5F),
        (mapPixelHeight * 0.5F) - centerY,
    };
}

std::string MakeResourceRelativeImagePath(const std::filesystem::path &mapPath,
                                          const std::string &tilesetImagePath) {
    namespace fs = std::filesystem;

    if (tilesetImagePath.empty()) {
        return {};
    }

    const fs::path imageAbsolutePath =
        fs::weakly_canonical(mapPath.parent_path() / fs::path(tilesetImagePath));
    const std::string imageAbsolute = imageAbsolutePath.generic_string();

    constexpr std::string_view marker = "/Resources/";
    const auto markerPos = imageAbsolute.find(marker);
    if (markerPos != std::string::npos) {
        return imageAbsolute.substr(markerPos + marker.size());
    }

    return imageAbsolute;
}

std::string ResolveTilesetImagePath(const tmx::Tileset &tileset,
                                    const std::uint32_t gid) {
    if (!tileset.getImagePath().empty()) {
        return tileset.getImagePath();
    }

    if (gid < tileset.getFirstGID()) {
        return {};
    }

    const std::uint32_t localId = gid - tileset.getFirstGID();
    for (const auto &tile : tileset.getTiles()) {
        if (tile.ID == localId && !tile.imagePath.empty()) {
            return tile.imagePath;
        }
    }

    return {};
}

std::uint32_t FindMyImagesFirstGid(const std::vector<tmx::Tileset> &tilesets) {
    for (const auto &tileset : tilesets) {
        if (tileset.getName() == "myimages") {
            return tileset.getFirstGID();
        }
    }
    return 0;
}

const tmx::Tileset *FindTilesetForGid(const std::vector<tmx::Tileset> &tilesets,
                                      const std::uint32_t gid) {
    if (tilesets.empty()) {
        return nullptr;
    }

    const tmx::Tileset *found = nullptr;
    std::uint32_t bestFirstGid = 0;
    for (const auto &tileset : tilesets) {
        const std::uint32_t firstGid = tileset.getFirstGID();
        if (firstGid <= gid && firstGid >= bestFirstGid) {
            bestFirstGid = firstGid;
            found = &tileset;
        }
    }

    return found;
}

glm::vec4 MakeUvRect(const tmx::Tileset &tileset, const std::uint32_t gid,
                     const std::uint8_t flipFlags) {
    const auto tileSize = tileset.getTileSize();
    const auto imageSize = tileset.getImageSize();

    if (tileSize.x == 0 || tileSize.y == 0 || imageSize.x == 0 || imageSize.y == 0) {
        return {0.0F, 0.0F, 1.0F, 1.0F};
    }

    const std::uint32_t localId = gid - tileset.getFirstGID();
    const std::uint32_t columns = std::max(tileset.getColumnCount(), 1u);
    const std::uint32_t col = localId % columns;
    const std::uint32_t row = localId / columns;

    const std::uint32_t spacing = tileset.getSpacing();
    const std::uint32_t margin = tileset.getMargin();

    const float px = static_cast<float>(margin + col * (tileSize.x + spacing));
    const float py = static_cast<float>(margin + row * (tileSize.y + spacing));

    float uMin = px / static_cast<float>(imageSize.x);
    float uMax = (px + static_cast<float>(tileSize.x)) / static_cast<float>(imageSize.x);

    float vMin = py / static_cast<float>(imageSize.y);
    float vMax = (py + static_cast<float>(tileSize.y)) / static_cast<float>(imageSize.y);

    if ((flipFlags & tmx::TileLayer::FlipFlag::Horizontal) != 0) {
        std::swap(uMin, uMax);
    }
    if ((flipFlags & tmx::TileLayer::FlipFlag::Vertical) != 0) {
        std::swap(vMin, vMax);
    }
    if ((flipFlags & tmx::TileLayer::FlipFlag::Diagonal) != 0) {
        LOG_WARN("Diagonal TMX tile flip is not fully supported yet.");
    }

    return {uMin, vMin, uMax, vMax};
}

float RenderLayerOrderToZIndex(const std::size_t renderLayerOrder) {
    const auto &cfg = GetTmxConfig();
    return cfg.backgroundZ + static_cast<float>(renderLayerOrder) * cfg.renderLayerZStep;
}

Game::LevelObjectConfig MakeInvisibleCollider(const glm::vec2 &position,
                                              const glm::vec2 &size,
                                              const float zIndex) {
    Game::LevelObjectConfig cfg{};
    cfg.position = position;
    cfg.size = size;
    cfg.zIndex = zIndex;
    cfg.visible = false;
    return cfg;
}
} // namespace

namespace Game {
LevelConfig LoadLevelFromTmx(const std::string &relativeMapPath) {
    const auto &cfg = GetTmxConfig();
    LevelConfig level{};
    level.mapPath = relativeMapPath;
    level.goalTexturePath = cfg.goalTexturePath;
    level.goalSize = cfg.goalDefaultSize;

    const std::string resolvedMapPath = Common::ResolveAssetPath(relativeMapPath);

    tmx::Map map;
    if (!map.load(resolvedMapPath)) {
        throw std::runtime_error("Failed to parse TMX map: " + resolvedMapPath);
    }

    const auto mapTileCount = map.getTileCount();
    const auto mapTileSize = map.getTileSize();
    const float mapPixelWidth =
        static_cast<float>(mapTileCount.x * mapTileSize.x);
    const float mapPixelHeight =
        static_cast<float>(mapTileCount.y * mapTileSize.y);
    level.mapPixelSize = {mapPixelWidth, mapPixelHeight};
    level.worldBoundsMin = {-mapPixelWidth * 0.5F, -mapPixelHeight * 0.5F};
    level.worldBoundsMax = {mapPixelWidth * 0.5F, mapPixelHeight * 0.5F};

    const auto &tilesets = map.getTilesets();
    const std::uint32_t myImagesFirstGid = FindMyImagesFirstGid(tilesets);

    auto decodeMyImagesLocal = [myImagesFirstGid](const std::uint32_t gid)
        -> std::optional<std::uint32_t> {
        if (myImagesFirstGid == 0 || gid < myImagesFirstGid) {
            return std::nullopt;
        }
        return (gid - myImagesFirstGid + 1);
    };

    bool spawnFound = false;
    bool goalFound = false;
    bool hasCustomBounds = false;
    float customBoundsMinX = std::numeric_limits<float>::max();
    float customBoundsMaxX = std::numeric_limits<float>::lowest();
    float customBoundsMinY = std::numeric_limits<float>::max();
    float customBoundsMaxY = std::numeric_limits<float>::lowest();
    std::size_t renderLayerOrder = 0;

    for (const auto &layerPtr : map.getLayers()) {
        if (layerPtr->getType() == tmx::Layer::Type::Tile) {
            const auto &layer = layerPtr->getLayerAs<tmx::TileLayer>();
            const bool isStationaryLayer = layer.getName() == cfg.stationaryLayerName;
            const bool isBreakableLayer = layer.getName() == cfg.breakableLayerName;
            if (!layer.getVisible()) {
                continue;
            }

            const float layerZ = RenderLayerOrderToZIndex(renderLayerOrder);
            ++renderLayerOrder;

            const auto layerOffset = layer.getOffset();

            const auto &tiles = layer.getTiles();
            if (tiles.empty()) {
                continue;
            }

            for (std::size_t i = 0; i < tiles.size(); ++i) {
                const auto &tile = tiles[i];
                if (tile.ID == 0) {
                    continue;
                }

                const std::uint32_t col = static_cast<std::uint32_t>(i % mapTileCount.x);
                const std::uint32_t row = static_cast<std::uint32_t>(i / mapTileCount.x);

                const float centerX = (static_cast<float>(col) + 0.5F) *
                                      static_cast<float>(mapTileSize.x) +
                                      static_cast<float>(layerOffset.x);
                const float centerY = (static_cast<float>(row) + 0.5F) *
                                      static_cast<float>(mapTileSize.y) +
                                      static_cast<float>(layerOffset.y);
                const glm::vec2 worldCenter =
                    TmxPixelCenterToWorld(centerX, centerY, mapPixelWidth, mapPixelHeight);

                if (isBreakableLayer) {
                    Game::LevelObjectConfig breakable{};
                    breakable.position = worldCenter;
                    breakable.size = {
                        static_cast<float>(mapTileSize.x),
                        static_cast<float>(mapTileSize.y),
                    };
                    breakable.zIndex = layerZ;
                    if (!cfg.breakableAnimation.frames.empty()) {
                        breakable.texturePath = cfg.breakableAnimation.frames.front();
                    }
                    breakable.animationFrames = cfg.breakableAnimation.frames;
                    breakable.animationIntervalMs = cfg.breakableAnimation.intervalMs;
                    breakable.visible = true;
                    level.breakableBlocks.push_back(std::move(breakable));
                    continue;
                }

                const auto localGid = decodeMyImagesLocal(tile.ID);

                if (isStationaryLayer && localGid.has_value()) {
                    if (localGid.value() == cfg.spawnLocalGid) {
                        level.spawn = worldCenter;
                        spawnFound = true;
                        continue;
                    }
                    if (localGid.value() == cfg.goalLocalGid) {
                        level.goalPosition = worldCenter;
                        level.goalSize = {
                            static_cast<float>(mapTileSize.x),
                            static_cast<float>(mapTileSize.y),
                        };
                        goalFound = true;
                        continue;
                    }
                    if (localGid.value() == cfg.disappearLocalGid) {
                        level.platforms.push_back(MakeInvisibleCollider(
                            worldCenter,
                            {static_cast<float>(mapTileSize.x), static_cast<float>(mapTileSize.y)},
                            1.0F));
                        continue;
                    }
                    if (localGid.value() >= cfg.shooterLocalGidMin &&
                        localGid.value() <= cfg.shooterLocalGidMax) {
                        level.deathZones.push_back(MakeInvisibleCollider(
                            worldCenter,
                            {static_cast<float>(mapTileSize.x), static_cast<float>(mapTileSize.y)},
                            3.0F));
                        continue;
                    }
                }

                const auto *tileset = FindTilesetForGid(tilesets, tile.ID);
                if (tileset == nullptr) {
                    continue;
                }

                const std::string tileImagePath = ResolveTilesetImagePath(*tileset, tile.ID);
                if (tileImagePath.empty()) {
                    LOG_WARN("Unable to resolve tileset image for gid {} in map {}",
                             tile.ID, resolvedMapPath);
                    continue;
                }

                LevelObjectConfig visual{};
                visual.position = worldCenter;
                visual.size = {
                    static_cast<float>(mapTileSize.x),
                    static_cast<float>(mapTileSize.y),
                };
                visual.zIndex = layerZ;
                visual.texturePath =
                    MakeResourceRelativeImagePath(std::filesystem::path(resolvedMapPath),
                                                  tileImagePath);
                visual.uvRect = MakeUvRect(*tileset, tile.ID, tile.flipFlags);
                visual.visible = true;

                level.renderTiles.push_back(visual);
            }
            continue;
        }

        if (layerPtr->getType() == tmx::Layer::Type::Image) {
            if (!layerPtr->getVisible()) {
                continue;
            }

            const float layerZ = RenderLayerOrderToZIndex(renderLayerOrder);
            ++renderLayerOrder;

            const auto &imageLayer = layerPtr->getLayerAs<tmx::ImageLayer>();
            const auto imageSize = imageLayer.getImageSize();
            if (imageLayer.getImagePath().empty() || imageSize.x == 0 || imageSize.y == 0) {
                continue;
            }

            const auto layerOffset = layerPtr->getOffset();
            const float centerX = static_cast<float>(layerOffset.x) +
                                  static_cast<float>(imageSize.x) * 0.5F;
            const float centerY = static_cast<float>(layerOffset.y) +
                                  static_cast<float>(imageSize.y) * 0.5F;

            LevelObjectConfig visual{};
            visual.position =
                TmxPixelCenterToWorld(centerX, centerY, mapPixelWidth, mapPixelHeight);
            visual.size = {
                static_cast<float>(imageSize.x),
                static_cast<float>(imageSize.y),
            };
            visual.zIndex = layerZ;
            visual.texturePath = MakeResourceRelativeImagePath(
                std::filesystem::path(resolvedMapPath), imageLayer.getImagePath());
            visual.visible = true;
            level.renderTiles.push_back(visual);
            continue;
        }

        if (layerPtr->getType() != tmx::Layer::Type::Object) {
            continue;
        }

        const auto &objectGroup = layerPtr->getLayerAs<tmx::ObjectGroup>();
        const bool isPlatformGroup = objectGroup.getName() == cfg.platformGroupName;
        const bool isSawGroup = objectGroup.getName() == cfg.sawGroupName;
        const bool isBoundsGroup = objectGroup.getName() == cfg.boundsGroupName;
        const auto layerOffset = layerPtr->getOffset();

        for (const auto &obj : objectGroup.getObjects()) {
            const auto aabb = obj.getAABB();
            const float centerX = aabb.left + static_cast<float>(layerOffset.x) +
                                  aabb.width * 0.5F;
            const float centerY = aabb.top + static_cast<float>(layerOffset.y) +
                                  aabb.height * 0.5F;
            const glm::vec2 worldCenter =
                TmxPixelCenterToWorld(centerX, centerY, mapPixelWidth, mapPixelHeight);
            const glm::vec2 size = {aabb.width, aabb.height};

            if (isBoundsGroup && size.x > 0.0F && size.y > 0.0F) {
                const glm::vec2 half = size * 0.5F;
                customBoundsMinX = std::min(customBoundsMinX, worldCenter.x - half.x);
                customBoundsMaxX = std::max(customBoundsMaxX, worldCenter.x + half.x);
                customBoundsMinY = std::min(customBoundsMinY, worldCenter.y - half.y);
                customBoundsMaxY = std::max(customBoundsMaxY, worldCenter.y + half.y);
                hasCustomBounds = true;
                continue;
            }

            if (isPlatformGroup) {
                level.platforms.push_back(MakeInvisibleCollider(worldCenter, size, 1.0F));
                continue;
            }

            if (isSawGroup || obj.getShape() == tmx::Object::Shape::Ellipse) {
                auto saw = MakeInvisibleCollider(worldCenter, size, 3.0F);
                if (!cfg.sawAnimation.frames.empty()) {
                    saw.texturePath = cfg.sawAnimation.frames.front();
                }
                saw.visible = true;
                saw.animationFrames = cfg.sawAnimation.frames;
                saw.animationIntervalMs = cfg.sawAnimation.intervalMs;
                level.deathZones.push_back(std::move(saw));
                continue;
            }

            if (obj.getName() == "spawn") {
                level.spawn = worldCenter;
                spawnFound = true;
                continue;
            }

            if (obj.getName() == "goal") {
                level.goalPosition = worldCenter;
                level.goalSize = size;
                goalFound = true;
            }
        }
    }

    if (hasCustomBounds && customBoundsMinX <= customBoundsMaxX &&
        customBoundsMinY <= customBoundsMaxY) {
        level.worldBoundsMin = {customBoundsMinX, customBoundsMinY};
        level.worldBoundsMax = {customBoundsMaxX, customBoundsMaxY};
    }

    if (!spawnFound) {
        level.spawn = TmxPixelCenterToWorld(cfg.fallbackSpawnOffset.x,
                                            mapPixelHeight - cfg.fallbackSpawnOffset.y,
                                            mapPixelWidth, mapPixelHeight);
        LOG_WARN("TMX spawn marker not found in {}, using fallback spawn.",
                 resolvedMapPath);
    }

    if (!goalFound) {
        level.goalPosition = TmxPixelCenterToWorld(
            mapPixelWidth - cfg.fallbackSpawnOffset.x, cfg.fallbackSpawnOffset.y,
                                                   mapPixelWidth, mapPixelHeight);
        LOG_WARN("TMX goal marker not found in {}, using fallback goal.",
                 resolvedMapPath);
    }

    const float boundaryWallThickness = cfg.boundaryWallThickness;
    const float boundaryWallHeight =
        (level.worldBoundsMax.y - level.worldBoundsMin.y) + cfg.boundaryWallExtraHeight;
    const float boundaryWallCenterY = (level.worldBoundsMin.y + level.worldBoundsMax.y) * 0.5F;

    LevelObjectConfig leftWall{};
    leftWall.position = {level.worldBoundsMin.x - boundaryWallThickness * 0.5F, boundaryWallCenterY};
    leftWall.size = {boundaryWallThickness, boundaryWallHeight};
    leftWall.zIndex = 1.0F;
    leftWall.visible = false;
    level.platforms.push_back(leftWall);

    LevelObjectConfig rightWall{};
    rightWall.position = {level.worldBoundsMax.x + boundaryWallThickness * 0.5F, boundaryWallCenterY};
    rightWall.size = {boundaryWallThickness, boundaryWallHeight};
    rightWall.zIndex = 1.0F;
    rightWall.visible = false;
    level.platforms.push_back(rightWall);

    return level;
}
} // namespace Game
