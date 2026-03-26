#include "App.hpp"
#include "Character.hpp"
#include "TileMap.hpp"
#include <algorithm>
#include <cmath>
#include <string>

#include <tmxlite/Layer.hpp>
#include <tmxlite/Map.hpp>
#include <tmxlite/TileLayer.hpp>

#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"

namespace {
constexpr float kPlayerScale = 0.2F;
} // namespace

void App::Start() {
    LOG_TRACE("Start");

    const std::string mapPath = std::string(RESOURCE_DIR) + "/factory1.tmx";
    if (!LoadCollisionMap(mapPath)) {
        m_CurrentState = State::END;
        return;
    }
    LoadVisualMap(mapPath);

    m_Player = std::make_shared<Character>(RESOURCE_DIR"/meatboystanding.png");
    m_Player->m_Transform.scale = {kPlayerScale, kPlayerScale};
    m_Player->m_Transform.translation = {0.0F, m_MapPixelHeight * 0.25F};
    m_Player->SetZIndex(10.0F);
    m_Renderer = Util::Renderer{};
    m_Renderer.AddChild(m_Player);

    m_CurrentState = State::UPDATE;
}

bool App::LoadCollisionMap(const std::string &mapPath) {
    tmx::Map map;
    if (!map.load(mapPath)) {
        LOG_ERROR("Failed to load collision map: {}", mapPath);
        return false;
    }

    const auto tileCount = map.getTileCount();
    const auto tileSize = map.getTileSize();
    const auto &layers = map.getLayers();

    m_MapWidth = tileCount.x;
    m_MapHeight = tileCount.y;
    m_TileWidth = static_cast<float>(tileSize.x);
    m_TileHeight = static_cast<float>(tileSize.y);
    m_MapPixelWidth = static_cast<float>(m_MapWidth) * m_TileWidth;
    m_MapPixelHeight = static_cast<float>(m_MapHeight) * m_TileHeight;
    m_SolidTiles.assign(m_MapWidth * m_MapHeight, false);

    bool usedStationaryLayer = false;
    std::size_t solidCount = 0;

    for (const auto &layer : layers) {
        if (layer->getType() != tmx::Layer::Type::Tile) {
            continue;
        }

        if (layer->getName() != "stationary") {
            continue;
        }

        usedStationaryLayer = true;

        const auto &tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();
        for (std::size_t i = 0; i < tiles.size() && i < m_SolidTiles.size(); ++i) {
            if (tiles[i].ID != 0) {
                if (!m_SolidTiles[i]) {
                    solidCount++;
                }
                m_SolidTiles[i] = true;
            }
        }
    }

    if (!usedStationaryLayer) {
        LOG_WARN("No 'stationary' layer found; collision map is empty.");
    }

    LOG_INFO("Collision map ready: {}x{} tiles, solid tiles={}",
             m_MapWidth,
             m_MapHeight,
             solidCount);

    return true;
}

bool App::LoadVisualMap(const std::string &mapPath) {
    tmx::Map map;
    if (!map.load(mapPath)) {
        LOG_ERROR("Failed to load visual map: {}", mapPath);
        return false;
    }

    const auto tileCount = map.getTileCount();
    const auto tileSize  = map.getTileSize();
    const int mapW  = static_cast<int>(tileCount.x);
    const int mapH  = static_cast<int>(tileCount.y);
    const int tileW = static_cast<int>(tileSize.x);
    const int tileH = static_cast<int>(tileSize.y);
    const int pixelW = mapW * tileW;
    const int pixelH = mapH * tileH;

    // --- Collect tileset surfaces ---
    struct TilesetEntry {
        uint32_t firstGID;
        uint32_t lastGID; // exclusive
        int cols;
        SDL_Surface *surf; // ARGB8888, owned here
    };

    const auto &tilesets = map.getTilesets();
    std::vector<TilesetEntry> entries;
    entries.reserve(tilesets.size());

    for (std::size_t i = 0; i < tilesets.size(); ++i) {
        const auto &ts = tilesets[i];
        const uint32_t lastGID = (i + 1 < tilesets.size())
                                     ? tilesets[i + 1].getFirstGID()
                                     : UINT32_MAX;

        // tmxlite returns absolute paths on macOS
        const std::string &imgPath = ts.getImagePath();
        SDL_Surface *raw = IMG_Load(imgPath.c_str());
        if (!raw) {
            LOG_WARN("TileMap: failed to load tileset '{}'", imgPath);
            entries.push_back({ts.getFirstGID(), lastGID, 0, nullptr});
            continue;
        }

        // Normalise to ARGB8888 so SdlFormatToGlFormat works and alpha is correct
        SDL_Surface *surf =
            SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(raw);

        const auto &imgSize = ts.getImageSize();
        const int cols = (tileW > 0) ? static_cast<int>(imgSize.x) / tileW : 1;
        entries.push_back({ts.getFirstGID(), lastGID, cols, surf});
    }

    auto findTileset = [&](uint32_t gid) -> TilesetEntry * {
        for (auto &e : entries) {
            if (gid >= e.firstGID && gid < e.lastGID) {
                return &e;
            }
        }
        return nullptr;
    };

    // --- Bake each tile layer into a single surface ---
    static const std::unordered_map<std::string, float> kLayerZ = {
        {"background", 0.0F},
        {"stationary", 2.0F},
        {"foreground", 8.0F},
    };

    const auto &layers = map.getLayers();
    for (const auto &layer : layers) {
        if (layer->getType() != tmx::Layer::Type::Tile) {
            continue;
        }
        const auto zIt = kLayerZ.find(layer->getName());
        if (zIt == kLayerZ.end()) {
            continue;
        }
        const float zIndex = zIt->second;

        const auto &tiles = layer->getLayerAs<tmx::TileLayer>().getTiles();

        SDL_Surface *baked = SDL_CreateRGBSurfaceWithFormat(
            0, pixelW, pixelH, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!baked) {
            LOG_WARN("TileMap: SDL_CreateRGBSurfaceWithFormat failed for layer '{}'",
                     layer->getName());
            continue;
        }
        // Fill with fully transparent black
        SDL_FillRect(baked, nullptr,
                     SDL_MapRGBA(baked->format, 0, 0, 0, 0));

        for (int row = 0; row < mapH; ++row) {
            for (int col = 0; col < mapW; ++col) {
                const std::size_t idx =
                    static_cast<std::size_t>(row * mapW + col);
                if (idx >= tiles.size()) {
                    continue;
                }
                const uint32_t gid = tiles[idx].ID;
                if (gid == 0) {
                    continue;
                }

                TilesetEntry *entry = findTileset(gid);
                if (!entry || !entry->surf) {
                    continue;
                }

                const uint32_t localID = gid - entry->firstGID;
                const int srcCol =
                    (entry->cols > 0)
                        ? static_cast<int>(localID) % entry->cols
                        : 0;
                const int srcRow =
                    (entry->cols > 0)
                        ? static_cast<int>(localID) / entry->cols
                        : 0;

                SDL_Rect srcRect = {srcCol * tileW, srcRow * tileH, tileW, tileH};
                SDL_Rect dstRect = {col  * tileW,   row  * tileH,  tileW, tileH};

                // BLENDMODE_NONE copies pixels as-is (preserves alpha)
                SDL_SetSurfaceBlendMode(entry->surf, SDL_BLENDMODE_NONE);
                SDL_BlitSurface(entry->surf, &srcRect, baked, &dstRect);
            }
        }

        // TileMap takes ownership of 'baked' and frees it after GPU upload
        auto drawable = std::make_shared<TileMap>(baked);
        auto obj = std::make_shared<Util::GameObject>(drawable, zIndex);
        m_Renderer.AddChild(obj);
        m_TileObjects.push_back(obj);
    }

    // Free tileset surfaces
    for (auto &e : entries) {
        if (e.surf) {
            SDL_FreeSurface(e.surf);
        }
    }

    LOG_INFO("Visual map loaded: {} layers", m_TileObjects.size());
    return true;
}

bool App::IsSolidTile(int col, int row) const {
    if (col < 0 || row < 0) {
        return false;
    }

    if (static_cast<std::size_t>(col) >= m_MapWidth ||
        static_cast<std::size_t>(row) >= m_MapHeight) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(row) * m_MapWidth +
                              static_cast<std::size_t>(col);
    return m_SolidTiles[index];
}

bool App::CollidesAt(const glm::vec2 &position) const {
    if (!m_Player) {
        return false;
    }

    const auto playerSize = m_Player->GetScaledSize();
    const glm::vec2 half = playerSize * 0.5F;

    const float left = position.x - half.x;
    const float right = position.x + half.x;
    const float top = position.y + half.y;
    const float bottom = position.y - half.y;

    const int colMin =
        static_cast<int>(std::floor((left + m_MapPixelWidth * 0.5F) / m_TileWidth));
    const int colMax =
        static_cast<int>(std::floor((right + m_MapPixelWidth * 0.5F) / m_TileWidth));
    const int rowMin =
        static_cast<int>(std::floor((m_MapPixelHeight * 0.5F - top) / m_TileHeight));
    const int rowMax =
        static_cast<int>(std::floor((m_MapPixelHeight * 0.5F - bottom) / m_TileHeight));

    for (int row = rowMin; row <= rowMax; ++row) {
        for (int col = colMin; col <= colMax; ++col) {
            if (IsSolidTile(col, row)) {
                return true;
            }
        }
    }

    return false;
}

void App::UpdatePlayerPhysics(float dtSeconds) {
    if (!m_Player) {
        return;
    }

    float inputX = 0.0F;
    if (Util::Input::IsKeyPressed(Util::Keycode::A) ||
        Util::Input::IsKeyPressed(Util::Keycode::LEFT)) {
        inputX -= 1.0F;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::D) ||
        Util::Input::IsKeyPressed(Util::Keycode::RIGHT)) {
        inputX += 1.0F;
    }

    m_PlayerVelocity.x = inputX * m_MoveSpeed;

    if (m_IsGrounded && Util::Input::IsKeyDown(Util::Keycode::SPACE)) {
        m_PlayerVelocity.y = m_JumpVelocity;
        m_IsGrounded = false;
    }

    m_PlayerVelocity.y += m_Gravity * dtSeconds;
    m_PlayerVelocity.y = std::max(m_PlayerVelocity.y, -m_MaxFallSpeed);

    const float moveX = m_PlayerVelocity.x * dtSeconds;
    const int xSteps = std::max(1, static_cast<int>(std::ceil(std::abs(moveX) / 4.0F)));
    const float stepX = moveX / static_cast<float>(xSteps);

    glm::vec2 position = m_Player->m_Transform.translation;
    for (int i = 0; i < xSteps; ++i) {
        const glm::vec2 next = {position.x + stepX, position.y};
        if (CollidesAt(next)) {
            m_PlayerVelocity.x = 0.0F;
            break;
        }
        position = next;
    }

    const float moveY = m_PlayerVelocity.y * dtSeconds;
    const int ySteps = std::max(1, static_cast<int>(std::ceil(std::abs(moveY) / 4.0F)));
    const float stepY = moveY / static_cast<float>(ySteps);

    m_IsGrounded = false;
    for (int i = 0; i < ySteps; ++i) {
        const glm::vec2 next = {position.x, position.y + stepY};
        if (CollidesAt(next)) {
            if (stepY < 0.0F) {
                m_IsGrounded = true;
            }
            m_PlayerVelocity.y = 0.0F;
            break;
        }
        position = next;
    }

    m_Player->m_Transform.translation = position;
}

void App::Update() {
    const float dtSeconds = Util::Time::GetDeltaTimeMs() / 1000.0F;
    UpdatePlayerPhysics(dtSeconds);

    m_Renderer.Update();

    /*
     * Do not touch the code below as they serve the purpose for
     * closing the window.
     */
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
        Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
