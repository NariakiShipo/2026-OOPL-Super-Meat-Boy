#include "App.hpp"

#include <filesystem>

#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

namespace {
std::string ResolveAssetPath(const std::string &relativePath) {
    const std::vector<std::string> candidates = {
        std::string(RESOURCE_DIR) + "/" + relativePath,
        "Resources/" + relativePath,
        "../Resources/" + relativePath,
        "PTSD/assets/" + relativePath,
        "../PTSD/assets/" + relativePath,
    };

    for (const auto &candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    LOG_WARN("Asset not found in known locations: {}", relativePath);
    return candidates.front();
}

std::vector<App::LevelConfig> BuildDefaultLevels() {
    using Obj = App::LevelObjectConfig;
    using Level = App::LevelConfig;

    Level level1;
    level1.spawn = {-520.0F, -40.0F};
    level1.goalPosition = {470.0F, 120.0F};
    level1.goalSize = {140.0F, 140.0F};
    level1.goalTexturePath = "images/bandagegirl.png";
    level1.platforms = {
        Obj{{0.0F, -320.0F}, {1300.0F, 70.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-220.0F, -120.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{220.0F, 20.0F}, {280.0F, 50.0F}, 2.0F, "images/sawshooter.png"},
    };
    level1.deathZones = {
        Obj{{-60.0F, -240.0F}, {180.0F, 40.0F}, 3.0F, "images/monsterts.png"},
        Obj{{360.0F, -260.0F}, {180.0F, 40.0F}, 3.0F, "images/monsterts.png"},
    };

    Level level2;
    level2.spawn = {-580.0F, -220.0F};
    level2.goalPosition = {560.0F, 240.0F};
    level2.goalSize = {140.0F, 140.0F};
    level2.goalTexturePath = "images/bandagegirl.png";
    level2.platforms = {
        Obj{{0.0F, -340.0F}, {1400.0F, 60.0F}, 1.0F, "images/disappearing.png"},
        Obj{{-350.0F, -120.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{-80.0F, 20.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{180.0F, 140.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
        Obj{{430.0F, 240.0F}, {220.0F, 45.0F}, 2.0F, "images/sawshooter.png"},
    };
    level2.deathZones = {
        Obj{{-210.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
        Obj{{70.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
        Obj{{340.0F, -270.0F}, {170.0F, 35.0F}, 3.0F, "images/monsterts.png"},
    };

    return {level1, level2};
}
} // namespace

std::shared_ptr<Util::GameObject> App::CreatePlatform(const glm::vec2 &position,
                                                      const glm::vec2 &size,
                                                      const float zIndex,
                                                      const std::string &texturePath) const {
    auto platform = std::make_shared<Util::GameObject>();
    auto sprite = std::make_shared<Util::Image>(ResolveAssetPath(texturePath));

    platform->SetDrawable(sprite);
    platform->m_Transform.translation = position;
    platform->SetZIndex(zIndex);

    auto spriteSize = sprite->GetSize();
    platform->m_Transform.scale = {
        size.x / spriteSize.x,
        size.y / spriteSize.y,
    };

    return platform;
}

void App::InitWorld() {
    m_Root = Util::Renderer();

    m_Player = std::make_shared<Util::GameObject>();

    m_PlayerIdleDrawable =
        std::make_shared<Util::Image>(ResolveAssetPath("images/meatboystanding.png"));
    m_PlayerRunLeftDrawable =
        std::make_shared<Util::Image>(ResolveAssetPath("images/sprintLeft.png"));
    m_PlayerRunRightDrawable =
        std::make_shared<Util::Image>(ResolveAssetPath("images/sprintRight.png"));

    // Resource pack has no dedicated jump/fall sprites, so reuse standing.
    m_PlayerJumpDrawable = m_PlayerIdleDrawable;
    m_PlayerFallDrawable = m_PlayerIdleDrawable;

    m_Player->SetDrawable(m_PlayerIdleDrawable);
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerFacingRight = true;
    m_Player->m_Transform.scale = {1.0F, 1.0F};
    m_PlayerColliderSize = m_PlayerIdleDrawable->GetSize() * m_Player->m_Transform.scale;
    m_Player->SetZIndex(10.0F);

    m_StatusBoard = std::make_shared<Util::GameObject>();
    m_StatusText = std::make_shared<Util::Text>(
        ResolveAssetPath("fonts/Inter.ttf"), 32, "Ready");
    m_StatusBoard->SetDrawable(m_StatusText);
    m_StatusBoard->SetZIndex(100.0F);

    m_Levels = BuildDefaultLevels();
    m_CurrentLevelIndex = 0;
    LoadLevel(m_CurrentLevelIndex);
}

void App::LoadLevel(const std::size_t levelIndex) {
    if (m_Levels.empty()) {
        return;
    }

    m_CurrentLevelIndex = levelIndex % m_Levels.size();
    const auto &level = m_Levels[m_CurrentLevelIndex];

    m_Root = Util::Renderer();
    m_Platforms.clear();
    m_DeathZones.clear();

    for (const auto &cfg : level.platforms) {
        m_Platforms.push_back(
            CreatePlatform(cfg.position, cfg.size, cfg.zIndex, cfg.texturePath));
    }

    for (const auto &cfg : level.deathZones) {
        m_DeathZones.push_back(
            CreatePlatform(cfg.position, cfg.size, cfg.zIndex, cfg.texturePath));
    }

    m_GoalFlag = CreatePlatform(level.goalPosition, level.goalSize, 4.0F,
                                level.goalTexturePath);

    for (const auto &platform : m_Platforms) {
        m_Root.AddChild(platform);
    }
    for (const auto &deathZone : m_DeathZones) {
        m_Root.AddChild(deathZone);
    }
    m_Root.AddChild(m_GoalFlag);
    m_Root.AddChild(m_Player);
    m_Root.AddChild(m_StatusBoard);

    m_PlayerSpawn = level.spawn;
    m_CameraPosition = m_PlayerSpawn;
    Util::SetCameraPosition(m_CameraPosition);

    m_LevelCleared = false;
    if (m_StatusText != nullptr) {
        m_StatusText->SetText("Reach the flag. Avoid red blocks.");
    }

    RespawnPlayer();
}

void App::Start() {
    LOG_TRACE("Start");

    InitWorld();

    m_CurrentState = State::UPDATE;
}