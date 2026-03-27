#include "App.hpp"

#include "common/AssetPath.hpp"
#include "game/LevelData.hpp"

#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

std::shared_ptr<Util::GameObject> App::CreatePlatform(const glm::vec2 &position,
                                                      const glm::vec2 &size,
                                                      const float zIndex,
                                                      const std::string &texturePath) const {
    auto platform = std::make_shared<Util::GameObject>();
    auto sprite =
        std::make_shared<Util::Image>(Common::ResolveAssetPath(texturePath));

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
    m_Player->m_Transform.scale = {1.0F, 1.0F};
    m_PlayerColliderSize = m_PlayerIdleDrawable->GetSize() * m_Player->m_Transform.scale;
    m_Player->SetZIndex(10.0F);

    m_StatusBoard = std::make_shared<Util::GameObject>();
    m_StatusText = std::make_shared<Util::Text>(
        Common::ResolveAssetPath("fonts/Inter.ttf"), 32, "Ready");
    m_StatusBoard->SetDrawable(m_StatusText);
    m_StatusBoard->SetZIndex(100.0F);

    m_Levels = Game::BuildDefaultLevels();
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