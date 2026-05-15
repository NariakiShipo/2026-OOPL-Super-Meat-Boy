#include "App.hpp"

#include <filesystem>

#include "common/AssetPath.hpp"
#include "game/BoxDrawable.hpp"
#include "game/Collision.hpp"
#include "game/LevelData.hpp"

#include "config.hpp"

#include "Util/Color.hpp"
#include "Util/BGM.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/SFX.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"
#include "Util/Animation.hpp"

namespace {
constexpr float kGoalSizeScale = 0.9F;
constexpr float kMinCameraTravelX = 140.0F;
constexpr float kMinCameraTravelY = 90.0F;
constexpr float kCameraZoomOutFactor = 0.7F;

float ComputeZoomForMinTravel(const float windowSize, const float worldSize,
                              const float minTravel) {
    const float clampedTravel = std::max(0.0F, minTravel);
    const float visibleSize = worldSize - clampedTravel;
    if (visibleSize <= 1.0F) {
        return std::numeric_limits<float>::infinity();
    }
    return windowSize / visibleSize;
}

float ClampToRangeOrCenter(const float value, const float minValue,
                           const float maxValue) {
    if (minValue > maxValue) {
        return (minValue + maxValue) * 0.5F;
    }
    return std::clamp(value, minValue, maxValue);
}

std::string ExtractLevelName(const std::string &mapPath) {
    const std::size_t slashPos = mapPath.find_last_of("/\\");
    const std::size_t nameStart =
        (slashPos == std::string::npos) ? 0 : (slashPos + 1);
    const std::size_t dotPos = mapPath.find_last_of('.');

    if (dotPos == std::string::npos || dotPos < nameStart) {
        return mapPath.substr(nameStart);
    }

    return mapPath.substr(nameStart, dotPos - nameStart);
}

std::shared_ptr<Util::GameObject> CreateTextObject(
    const std::string &fontPath, const int fontSize, const std::string &text,
    const glm::vec2 &translation, const float zIndex,
    const Util::Color &color = Util::Color(255, 255, 255, 255)) {
    auto object = std::make_shared<Util::GameObject>();
    object->SetDrawable(std::make_shared<Util::Text>(fontPath, fontSize, text, color));
    object->m_Transform.translation = translation;
    object->SetZIndex(zIndex);
    return object;
}
} // namespace

std::shared_ptr<Util::GameObject> App::CreatePlatform(const glm::vec2 &position,
                                                      const glm::vec2 &size,
                                                      const float zIndex,
                                                      const std::string &texturePath,
                                                      const glm::vec4 &uvRect,
                                                      const bool visible) const {
    auto platform = std::make_shared<Util::GameObject>();

    if (texturePath.empty()) {
        platform->SetDrawable(std::make_shared<Game::BoxDrawable>(size));
        platform->m_Transform.scale = {1.0F, 1.0F};
    } else {
        const bool hasAbsolutePath =
            !texturePath.empty() &&
            (texturePath.front() == '/' || texturePath.find(':') != std::string::npos);
        const std::string resolvedPath =
            hasAbsolutePath ? texturePath : Common::ResolveAssetPath(texturePath);

        auto sprite = std::make_shared<Util::Image>(resolvedPath);
        sprite->SetUVRect(uvRect);
        platform->SetDrawable(sprite);

        auto spriteSize = sprite->GetSize();
        platform->m_Transform.scale = {
            size.x / spriteSize.x,
            size.y / spriteSize.y,
        };
    }

    platform->m_Transform.translation = position;
    platform->SetZIndex(zIndex);
    platform->SetVisible(visible);

    return platform;
}

void App::InitWorld() {
    m_Root = Util::Renderer();

    m_Player = std::make_shared<Util::GameObject>();

    m_PlayerIdleDrawable =
        std::make_shared<Util::Image>(Common::ResolveAssetPath("images/meatboystanding.png"));
    m_PlayerRunLeftDrawable =
        std::make_shared<Util::Image>(Common::ResolveAssetPath("images/sprintLeft.png"));
    m_PlayerRunRightDrawable =
        std::make_shared<Util::Image>(Common::ResolveAssetPath("images/sprintRight.png"));

    m_PlayerJumpDrawable = m_PlayerIdleDrawable;
    m_PlayerFallDrawable = m_PlayerIdleDrawable;

    m_Player->SetDrawable(m_PlayerIdleDrawable);
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerFacingRight = true;
    m_Player->m_Transform.scale = {0.8F, 0.8F};
    m_PlayerColliderSize = m_PlayerIdleDrawable->GetSize() * m_Player->m_Transform.scale;
    m_Player->SetZIndex(10.0F);

    m_StatusBoard = std::make_shared<Util::GameObject>();
    m_StatusText = std::make_shared<Util::Text>(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 32, "Ready");
    m_StatusBoard->SetDrawable(m_StatusText);
    m_StatusBoard->SetZIndex(100.0F);

    m_TitleBackground = std::make_shared<Util::GameObject>();
    auto titleBackground = std::make_shared<Util::Image>(
        Common::ResolveAssetPath("images/titlescreen.png"));
    m_TitleBackground->SetDrawable(titleBackground);
    const auto titleBackgroundSize = titleBackground->GetSize();
    m_TitleBackground->m_Transform.scale = {
        static_cast<float>(WINDOW_WIDTH) / titleBackgroundSize.x,
        static_cast<float>(WINDOW_HEIGHT) / titleBackgroundSize.y,
    };
    m_TitleBackground->SetZIndex(-100.0F);

    m_StartButton = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 40, "start game",
        {0.0F, -120.0F}, 130.0F, Util::Color(255, 255, 255, 255));

    m_SettingsButton = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 40, "settings",
        {0.0F, -190.0F}, 130.0F, Util::Color(255, 255, 255, 255));

    m_SettingsTitleText = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 44, "settings",
        {0.0F, 130.0F}, 210.0F, Util::Color(255, 244, 200, 255));

    m_BgmSettingText = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 30, "bgm", {0.0F, 30.0F},
        210.0F, Util::Color(255, 255, 255, 255));

    m_SfxSettingText = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 30, "sfx", {0.0F, -60.0F},
        210.0F, Util::Color(255, 255, 255, 255));

    m_SettingsBackButton = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 36, "back",
        {0.0F, -210.0F}, 210.0F, Util::Color(255, 255, 255, 255));

    m_SettingsHelpText = CreateTextObject(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 22,
        "drag the bars to set bgm and sfx volume", {0.0F, -270.0F}, 210.0F,
        Util::Color(220, 220, 220, 255));

    m_TitleScreenObjects = {
        m_TitleBackground,
        m_StartButton,
        m_SettingsButton,
    };

    m_SettingsObjects = {
        m_SettingsTitleText,
        m_BgmSettingText,
        m_SfxSettingText,
        m_SettingsBackButton,
        m_SettingsHelpText,
    };

    m_TitleBGM = std::make_shared<Util::BGM>(
        Common::ResolveAssetPath("audio/testbgm.mp3"));
    m_ButtonSFX =
        std::make_shared<Util::SFX>(Common::ResolveAssetPath("audio/Click.wav"));

    m_AudioSettingsPath = std::filesystem::current_path() / "audio_settings.txt";
    LoadAudioSettings();
    ApplyAudioSettings();
    m_TitleBGM->Play(-1);

    m_Levels = Game::BuildDefaultLevels();
    m_CurrentLevelIndex = 0;
    ShowTitleScreen();
}

void App::LoadLevel(const std::size_t levelIndex) {
    if (m_Levels.empty()) {
        return;
    }

    m_CurrentLevelIndex = levelIndex % m_Levels.size();
    const auto &level = m_Levels[m_CurrentLevelIndex];

    m_Root = Util::Renderer();
    m_Platforms.clear();
    m_BreakableBlocks.clear();
    m_DeathZones.clear();
    m_LevelRenderTiles.clear();

    for (const auto &cfg : level.renderTiles) {
        m_LevelRenderTiles.push_back(
            CreatePlatform(cfg.position, cfg.size, cfg.zIndex, cfg.texturePath,
                           cfg.uvRect, cfg.visible));
    }

    for (const auto &cfg : level.platforms) {
        m_Platforms.push_back(
            CreatePlatform(cfg.position, cfg.size, cfg.zIndex, cfg.texturePath,
                           cfg.uvRect, cfg.visible));
    }

    for (const auto &cfg : level.breakableBlocks) {
        auto block = std::make_shared<Util::GameObject>();

        std::vector<std::string> resolvedPaths;
        resolvedPaths.reserve(cfg.animationFrames.size());
        for (const auto &framePath : cfg.animationFrames) {
            const bool hasAbsolutePath =
                !framePath.empty() &&
                (framePath.front() == '/' || framePath.find(':') != std::string::npos);
            resolvedPaths.push_back(
                hasAbsolutePath ? framePath : Common::ResolveAssetPath(framePath));
        }

        const std::size_t intervalMs = std::max<std::size_t>(cfg.animationIntervalMs, 1);
        auto animation = std::make_shared<Util::Animation>(resolvedPaths, false,
                                                            intervalMs, false, 0);
        block->SetDrawable(animation);

        const auto frameSize = animation->GetSize();
        block->m_Transform.scale = {
            cfg.size.x / frameSize.x,
            cfg.size.y / frameSize.y,
        };
        block->m_Transform.translation = cfg.position;
        block->SetZIndex(cfg.zIndex);
        block->SetVisible(cfg.visible);

        m_BreakableBlocks.push_back({block, animation, cfg.size, false, false});
    }

    if (m_BreakableBlocks.empty()) {
        LOG_WARN("No breakable blocks loaded for level '{}'", level.mapPath);
    } else {
        LOG_INFO("Loaded {} breakable block(s) for level '{}'",
                 m_BreakableBlocks.size(), level.mapPath);
        for (std::size_t i = 0; i < m_BreakableBlocks.size(); ++i) {
            const auto &obj = m_BreakableBlocks[i].object;
            if (obj == nullptr) {
                continue;
            }
            const auto size = obj->GetScaledSize();
            const auto pos = obj->m_Transform.translation;
            LOG_DEBUG("Breakable[{}] pos=({}, {}), size=({}, {})",
                      i, pos.x, pos.y, size.x, size.y);
        }
    }

    for (const auto &cfg : level.deathZones) {
        if (!cfg.animationFrames.empty()) {
            auto hazard = std::make_shared<Util::GameObject>();

            std::vector<std::string> resolvedPaths;
            resolvedPaths.reserve(cfg.animationFrames.size());
            for (const auto &framePath : cfg.animationFrames) {
                const bool hasAbsolutePath =
                    !framePath.empty() &&
                    (framePath.front() == '/' ||
                     framePath.find(':') != std::string::npos);
                resolvedPaths.push_back(
                    hasAbsolutePath ? framePath : Common::ResolveAssetPath(framePath));
            }

            const std::size_t intervalMs = std::max<std::size_t>(cfg.animationIntervalMs, 1);
            auto animatedSaw =
                std::make_shared<Util::Animation>(resolvedPaths, true, intervalMs, true, 0);
            hazard->SetDrawable(animatedSaw);

            const auto frameSize = animatedSaw->GetSize();
            hazard->m_Transform.scale = {
                cfg.size.x / frameSize.x,
                cfg.size.y / frameSize.y,
            };
            hazard->m_Transform.translation = cfg.position;
            hazard->SetZIndex(cfg.zIndex);
            hazard->SetVisible(cfg.visible);

            m_DeathZones.push_back(hazard);
            continue;
        }

        m_DeathZones.push_back(CreatePlatform(cfg.position, cfg.size, cfg.zIndex,
                                              cfg.texturePath, cfg.uvRect,
                                              cfg.visible));
    }

    m_GoalFlag = CreatePlatform(level.goalPosition, level.goalSize * kGoalSizeScale, 4.0F,
                                level.goalTexturePath);

    m_PlayerSpawn = level.spawn;
    m_WorldBoundsMin = level.worldBoundsMin;
    m_WorldBoundsMax = level.worldBoundsMax;
    m_CameraLookaheadOffset = {0.0F, 0.0F};

    const float zoomX = (level.mapPixelSize.x > 0.0F)
                            ? static_cast<float>(WINDOW_WIDTH) / level.mapPixelSize.x
                            : 1.0F;
    const float zoomY = (level.mapPixelSize.y > 0.0F)
                            ? static_cast<float>(WINDOW_HEIGHT) / level.mapPixelSize.y
                            : 1.0F;
    m_CameraZoom = std::min(zoomX, zoomY);

    const float worldWidth = m_WorldBoundsMax.x - m_WorldBoundsMin.x;
    const float worldHeight = m_WorldBoundsMax.y - m_WorldBoundsMin.y;
    const float minZoomForTravelX =
        ComputeZoomForMinTravel(static_cast<float>(WINDOW_WIDTH), worldWidth,
                                kMinCameraTravelX);
    const float minZoomForTravelY =
        ComputeZoomForMinTravel(static_cast<float>(WINDOW_HEIGHT), worldHeight,
                                kMinCameraTravelY);

    if (std::isfinite(minZoomForTravelX)) {
        m_CameraZoom = std::max(m_CameraZoom, minZoomForTravelX);
    }
    if (std::isfinite(minZoomForTravelY)) {
        m_CameraZoom = std::max(m_CameraZoom, minZoomForTravelY);
    }

    m_CameraZoom *= kCameraZoomOutFactor;

    if (!std::isfinite(m_CameraZoom) || m_CameraZoom <= 0.0F) {
        m_CameraZoom = 1.0F;
    }
    Util::SetCameraZoom(m_CameraZoom);

    Game::Aabb worldBounds = {
        m_WorldBoundsMin.x,
        m_WorldBoundsMax.x,
        m_WorldBoundsMin.y,
        m_WorldBoundsMax.y,
    };

    const glm::vec2 cameraHalfWindow = {
        static_cast<float>(WINDOW_WIDTH) * 0.5F / m_CameraZoom,
        static_cast<float>(WINDOW_HEIGHT) * 0.5F / m_CameraZoom,
    };

    const float minCameraX = worldBounds.minX + cameraHalfWindow.x;
    const float maxCameraX = worldBounds.maxX - cameraHalfWindow.x;
    const float minCameraY = worldBounds.minY + cameraHalfWindow.y;
    const float maxCameraY = worldBounds.maxY - cameraHalfWindow.y;

    if (minCameraX <= maxCameraX) {
        m_CameraBoundsMin.x = minCameraX;
        m_CameraBoundsMax.x = maxCameraX;
    } else {
        const float centerX = (worldBounds.minX + worldBounds.maxX) * 0.5F;
        m_CameraBoundsMin.x = centerX;
        m_CameraBoundsMax.x = centerX;
    }

    if (minCameraY <= maxCameraY) {
        m_CameraBoundsMin.y = minCameraY;
        m_CameraBoundsMax.y = maxCameraY;
    } else {
        const float centerY = (worldBounds.minY + worldBounds.maxY) * 0.5F;
        m_CameraBoundsMin.y = centerY;
        m_CameraBoundsMax.y = centerY;
    }

    m_CameraPosition.x = ClampToRangeOrCenter(m_PlayerSpawn.x, m_CameraBoundsMin.x,
                                              m_CameraBoundsMax.x);
    m_CameraPosition.y = ClampToRangeOrCenter(m_PlayerSpawn.y, m_CameraBoundsMin.y,
                                              m_CameraBoundsMax.y);
    Util::SetCameraPosition(m_CameraPosition);

    m_LevelCleared = false;
    if (m_StatusText != nullptr) {
        m_StatusText->SetText(ExtractLevelName(level.mapPath));
    }

    ShowGameplayScreen();
    RespawnPlayer();
}

void App::Start() {
    LOG_TRACE("Start");

    InitWorld();

    m_CurrentState = State::TITLE;
}