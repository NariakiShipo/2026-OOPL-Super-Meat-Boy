#include "App.hpp"

#include <filesystem>

#include "common/AssetPath.hpp"
#include "game/BoxDrawable.hpp"
#include "game/Collision.hpp"
#include "game/LevelData.hpp"
#include "game/TmxLoader.hpp"

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
    LoadGameConfig();
    m_Root = Util::Renderer();

    m_Player = std::make_shared<Util::GameObject>();

    m_PlayerIdleDrawable = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(m_Config.player.idleSpritePath));
    m_PlayerRunLeftDrawable = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(m_Config.player.runLeftSpritePath));
    m_PlayerRunRightDrawable = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(m_Config.player.runRightSpritePath));

    m_PlayerJumpDrawable = m_PlayerIdleDrawable;
    m_PlayerFallDrawable = m_PlayerIdleDrawable;

    m_Player->SetDrawable(m_PlayerIdleDrawable);
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerFacingRight = true;
    m_Player->m_Transform.scale = {m_Config.player.scale, m_Config.player.scale};
    m_PlayerColliderSize = m_PlayerIdleDrawable->GetSize() * m_Player->m_Transform.scale;
    m_Player->SetZIndex(m_Config.player.zIndex);

    m_StatusBoard = std::make_shared<Util::GameObject>();
    m_StatusText = std::make_shared<Util::Text>(
        Common::ResolveAssetPath(m_Config.ui.statusText.fontPath),
        m_Config.ui.statusText.fontSize,
        m_Config.ui.statusText.text,
        m_Config.ui.statusText.color);
    m_StatusBoard->SetDrawable(m_StatusText);
    m_StatusBoard->SetZIndex(m_Config.ui.statusText.zIndex);

    // 作弊模式提示字（預設隱藏，F2 開啟後顯示，位置每幀跟著相機）。
    m_CheatIndicator = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.statusText.fontPath), 28,
        "CHEAT: SAW IMMUNE (F2)", {0.0F, -330.0F},
        m_Config.ui.statusText.zIndex, Util::Color(255, 200, 60, 255));
    m_CheatIndicator->SetVisible(false);

    // HUD：關卡計時器與繃帶數量（細節由 gameplay.json 的 ui.timerText / ui.bandageText 設定）。
    m_TimerText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.timerText.fontPath),
        m_Config.ui.timerText.fontSize, m_Config.ui.timerText.text,
        m_Config.ui.timerText.position, m_Config.ui.timerText.zIndex,
        m_Config.ui.timerText.color);
    m_BandageCountText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.bandageText.fontPath),
        m_Config.ui.bandageText.fontSize, m_Config.ui.bandageText.text,
        m_Config.ui.bandageText.position, m_Config.ui.bandageText.zIndex,
        m_Config.ui.bandageText.color);

    m_TitleBackground = std::make_shared<Util::GameObject>();
    auto titleBackground = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(m_Config.ui.titleBackgroundPath));
    m_TitleBackground->SetDrawable(titleBackground);
    const auto titleBackgroundSize = titleBackground->GetSize();
    m_TitleBackground->m_Transform.scale = {
        static_cast<float>(WINDOW_WIDTH) / titleBackgroundSize.x,
        static_cast<float>(WINDOW_HEIGHT) / titleBackgroundSize.y,
    };
    m_TitleBackground->SetZIndex(m_Config.ui.titleBackgroundZ);

    m_StartButton = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.startButton.fontPath),
        m_Config.ui.startButton.fontSize,
        m_Config.ui.startButton.text,
        m_Config.ui.startButton.position,
        m_Config.ui.startButton.zIndex,
        m_Config.ui.startButton.color);

    m_SettingsButton = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.settingsButton.fontPath),
        m_Config.ui.settingsButton.fontSize,
        m_Config.ui.settingsButton.text,
        m_Config.ui.settingsButton.position,
        m_Config.ui.settingsButton.zIndex,
        m_Config.ui.settingsButton.color);

    m_SettingsTitleText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.settingsTitle.fontPath),
        m_Config.ui.settingsTitle.fontSize,
        m_Config.ui.settingsTitle.text,
        m_Config.ui.settingsTitle.position,
        m_Config.ui.settingsTitle.zIndex,
        m_Config.ui.settingsTitle.color);

    m_BgmSettingText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.bgmLabel.fontPath),
        m_Config.ui.bgmLabel.fontSize,
        m_Config.ui.bgmLabel.text,
        m_Config.ui.bgmLabel.position,
        m_Config.ui.bgmLabel.zIndex,
        m_Config.ui.bgmLabel.color);

    m_SfxSettingText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.sfxLabel.fontPath),
        m_Config.ui.sfxLabel.fontSize,
        m_Config.ui.sfxLabel.text,
        m_Config.ui.sfxLabel.position,
        m_Config.ui.sfxLabel.zIndex,
        m_Config.ui.sfxLabel.color);

    m_SettingsBackButton = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.backButton.fontPath),
        m_Config.ui.backButton.fontSize,
        m_Config.ui.backButton.text,
        m_Config.ui.backButton.position,
        m_Config.ui.backButton.zIndex,
        m_Config.ui.backButton.color);

    m_SettingsHelpText = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.helpText.fontPath),
        m_Config.ui.helpText.fontSize,
        m_Config.ui.helpText.text,
        m_Config.ui.helpText.position,
        m_Config.ui.helpText.zIndex,
        m_Config.ui.helpText.color);

    m_SettingsToLevelSelectButton = CreateTextObject(
        Common::ResolveAssetPath(m_Config.ui.toLevelSelectButton.fontPath),
        m_Config.ui.toLevelSelectButton.fontSize,
        m_Config.ui.toLevelSelectButton.text,
        m_Config.ui.toLevelSelectButton.position,
        m_Config.ui.toLevelSelectButton.zIndex,
        m_Config.ui.toLevelSelectButton.color);

    m_TitleScreenObjects = {
        m_TitleBackground,
        m_StartButton,
        m_SettingsButton,
    };

    // 設定頁底板（在遊戲中開啟也看得清楚）
    auto settingsPanel = CreatePlatform({0.0F, -90.0F}, {840.0F, 600.0F},
                                        205.0F, "images/ui_panel.png");

    m_SettingsObjects = {
        settingsPanel,
        m_SettingsTitleText,
        m_BgmSettingText,
        m_SfxSettingText,
        m_SettingsBackButton,
        m_SettingsHelpText,
        m_SettingsToLevelSelectButton,
    };

    // 記錄基準 transform：OpenSettingsMenu 依目前相機重新放置。
    // 底板中心原本在 y=-90（偏螢幕下方），整組（底板＋所有文字）往上平移，
    // 讓設定選單垂直置中於螢幕。
    constexpr float kSettingsCenterOffsetY = 90.0F;
    m_SettingsBasePositions.clear();
    m_SettingsBaseScales.clear();
    for (const auto &object : m_SettingsObjects) {
        glm::vec2 basePosition = object != nullptr
                                     ? object->m_Transform.translation
                                     : glm::vec2{0.0F, 0.0F};
        basePosition.y += kSettingsCenterOffsetY;
        m_SettingsBasePositions.push_back(basePosition);
        m_SettingsBaseScales.push_back(object != nullptr
                                           ? object->m_Transform.scale
                                           : glm::vec2{1.0F, 1.0F});
    }

    m_TitleBGM = std::make_shared<Util::BGM>(
        Common::ResolveAssetPath(m_Config.audio.titleBgmPath));
    m_ButtonSFX =
        std::make_shared<Util::SFX>(Common::ResolveAssetPath(m_Config.audio.buttonSfxPath));

    m_AudioSettingsPath = std::filesystem::current_path() / "audio_settings.txt";
    LoadAudioSettings();
    ApplyAudioSettings();
    m_TitleBGM->Play(-1);

    BuildPauseMenu();

    m_Worlds = Game::BuildWorldData();
    m_Levels = Game::BuildDefaultLevels();

    // Boss 關（步驟3）：優先載入 forestboss.tmx 完整地形，
    // 失敗則退回步驟1的手刻測試關；選關畫面按 B 進入
    try {
        auto bossLevel = Game::LoadLevelFromTmx("images/forestboss.tmx");
        bossLevel.worldInfo = {Game::WorldCategory::Forest, "Forest",
                               "images/forestlevelselect.png", 8};
        m_Levels.push_back(std::move(bossLevel));
        LOG_INFO("Loaded boss level from forestboss.tmx");
    } catch (const std::exception &e) {
        LOG_WARN("forestboss.tmx load failed ({}); using built-in test level",
                 e.what());
        m_Levels.push_back(Game::BuildBossTestLevel());
    }
    m_BossTestLevelIndex = m_Levels.size() - 1;

    m_CurrentLevelIndex = 0;
    m_LevelSelectWorldIndex = 0;
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
    m_Shooters.clear();
    m_LiveBuzzsaws.clear();
    m_Rotors.clear();

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

    m_GoalFlag = CreatePlatform(level.goalPosition,
                                level.goalSize * m_Config.goalSizeScale,
                                4.0F,
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
                                m_Config.camera.minTravelX);
    const float minZoomForTravelY =
        ComputeZoomForMinTravel(static_cast<float>(WINDOW_HEIGHT), worldHeight,
                                m_Config.camera.minTravelY);

    if (std::isfinite(minZoomForTravelX)) {
        m_CameraZoom = std::max(m_CameraZoom, minZoomForTravelX);
    }
    if (std::isfinite(minZoomForTravelY)) {
        m_CameraZoom = std::max(m_CameraZoom, minZoomForTravelY);
    }

    m_CameraZoom *= m_Config.camera.zoomOutFactor;

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
        // 視野比地圖還高時，把鏡頭頂端對齊地圖上邊界（而非置中），
        // 讓多出來的空白落在地圖下方，避免鏡頭在上方露出黑邊。
        // maxCameraY = worldBoundsMax.y - cameraHalfWindow.y，
        // 鏡頭位在此處時，畫面上緣剛好等於地圖頂端。
        m_CameraBoundsMin.y = maxCameraY;
        m_CameraBoundsMax.y = maxCameraY;
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

    // Initialise shooters; timer starts at intervalMs so first shot
    // fires after one full interval (gives player reaction time)
    for (const auto &sc : level.shooters) {
        m_Shooters.push_back({sc, m_Config.buzzsaw.intervalMs});
    }
    LOG_INFO("Loaded {} shooter(s) for level '{}'",
             m_Shooters.size(), level.mapPath);

    // 旋轉鋸（ShowGameplayScreen 內會掛進 renderer）
    SpawnRotors(level.rotors);

    // 繃帶收集品（位置來自 TMX；ShowGameplayScreen 內會掛進 renderer）
    SpawnBandages(level.bandages);

    // Boss 關卡參數 + 生成（ShowGameplayScreen 內會掛進 renderer）
    m_LevelHasBoss = level.boss.enabled;
    m_BossSpawnPoint = level.boss.spawn;
    m_BossRushTriggerX = level.boss.rushTriggerX;
    m_BossCrashX = level.boss.crashX;
    SpawnBoss();

    ShowGameplayScreen();
    RespawnPlayer();
}

void App::Start() {
    LOG_TRACE("Start");

    InitWorld();

    m_CurrentState = State::TITLE;
}