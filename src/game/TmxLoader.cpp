#include "game/TmxLoader.hpp"

#include <filesystem>

#include <tmxlite/Map.hpp>
#include <tmxlite/ImageLayer.hpp>
#include <tmxlite/Object.hpp>
#include <tmxlite/ObjectGroup.hpp>
#include <tmxlite/TileLayer.hpp>

#include "Util/Logger.hpp"
#include "common/AssetPath.hpp"

namespace {
constexpr std::uint32_t kSpawnLocalGid = 1;
constexpr std::uint32_t kGoalLocalGid = 2;
constexpr std::uint32_t kDisappearLocalGid = 3;
constexpr std::uint32_t kShooterLocalGidMin = 13;
constexpr std::uint32_t kShooterLocalGidMax = 16;

constexpr float kBackgroundZ = -20.0F;
constexpr float kStationaryZ = -5.0F;
constexpr float kForegroundZ = 8.0F;

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

float LayerToZIndex(const std::string &name) {
    if (name == "background") {
        return kBackgroundZ;
    }
    if (name == "foreground") {
        return kForegroundZ;
    }
    return kStationaryZ;
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
    LevelConfig level{};
    level.mapPath = relativeMapPath;
    level.goalTexturePath = "images/bandagegirl.png";
    level.goalSize = {96.0F, 96.0F};

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

    for (const auto &layerPtr : map.getLayers()) {
        if (layerPtr->getType() == tmx::Layer::Type::Tile) {
            const auto &layer = layerPtr->getLayerAs<tmx::TileLayer>();
            const bool isStationaryLayer = layer.getName() == "stationary";
            if (!layer.getVisible()) {
                continue;
            }

            const auto layerOffset = layer.getOffset();

            const auto &tiles = layer.getTiles();
            if (tiles.empty()) {
                continue;
            }

            const float layerZ = LayerToZIndex(layer.getName());
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

                const auto localGid = decodeMyImagesLocal(tile.ID);

                if (isStationaryLayer && localGid.has_value()) {
                    if (localGid.value() == kSpawnLocalGid) {
                        level.spawn = worldCenter;
                        spawnFound = true;
                        continue;
                    }
                    if (localGid.value() == kGoalLocalGid) {
                        level.goalPosition = worldCenter;
                        level.goalSize = {
                            static_cast<float>(mapTileSize.x),
                            static_cast<float>(mapTileSize.y),
                        };
                        goalFound = true;
                        continue;
                    }
                    if (localGid.value() == kDisappearLocalGid) {
                        level.platforms.push_back(MakeInvisibleCollider(
                            worldCenter,
                            {static_cast<float>(mapTileSize.x), static_cast<float>(mapTileSize.y)},
                            1.0F));
                        continue;
                    }
                    if (localGid.value() >= kShooterLocalGidMin &&
                        localGid.value() <= kShooterLocalGidMax) {
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
            visual.zIndex = LayerToZIndex(layerPtr->getName());
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
        const bool isPlatformGroup = objectGroup.getName() == "rectangle";
        const bool isSawGroup = objectGroup.getName() == "saws";
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

            if (isPlatformGroup) {
                level.platforms.push_back(MakeInvisibleCollider(worldCenter, size, 1.0F));
                continue;
            }

            if (isSawGroup || obj.getShape() == tmx::Object::Shape::Ellipse) {
                level.deathZones.push_back(MakeInvisibleCollider(worldCenter, size, 3.0F));
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

    if (!spawnFound) {
        level.spawn = TmxPixelCenterToWorld(80.0F, mapPixelHeight - 80.0F,
                                            mapPixelWidth, mapPixelHeight);
        LOG_WARN("TMX spawn marker not found in {}, using fallback spawn.",
                 resolvedMapPath);
    }

    if (!goalFound) {
        level.goalPosition = TmxPixelCenterToWorld(mapPixelWidth - 80.0F, 80.0F,
                                                   mapPixelWidth, mapPixelHeight);
        LOG_WARN("TMX goal marker not found in {}, using fallback goal.",
                 resolvedMapPath);
    }

    return level;
}
} // namespace Game
