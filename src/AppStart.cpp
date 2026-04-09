#include "App.hpp"

#include "common/AssetPath.hpp"
#include "game/Collision.hpp"
#include "game/LevelData.hpp"

#include "config.hpp"

#include "Util/Image.hpp"
#include "Util/Logger.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

namespace {
void ExpandAabb(Game::Aabb &target, const Game::Aabb &incoming) {
    target.minX = std::min(target.minX, incoming.minX);
    target.maxX = std::max(target.maxX, incoming.maxX);
    target.minY = std::min(target.minY, incoming.minY);
    target.maxY = std::max(target.maxY, incoming.maxY);
}

float ClampToRangeOrCenter(const float value, const float minValue,
                           const float maxValue) {
    if (minValue > maxValue) {
        return (minValue + maxValue) * 0.5F;
    }
    return std::clamp(value, minValue, maxValue);
}
} // namespace

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
    m_CameraLookaheadOffset = {0.0F, 0.0F};

    auto worldBounds = Game::MakeAabb(m_PlayerSpawn, m_PlayerColliderSize);
    for (const auto &platform : m_Platforms) {
        ExpandAabb(worldBounds, Game::GetAabb(platform));
    }
    if (m_GoalFlag != nullptr) {
        ExpandAabb(worldBounds, Game::GetAabb(m_GoalFlag));
    }

    const glm::vec2 halfWindow = {
        static_cast<float>(WINDOW_WIDTH) * 0.5F,
        static_cast<float>(WINDOW_HEIGHT) * 0.5F,
    };

    const float minCameraX = worldBounds.minX + halfWindow.x;
    const float maxCameraX = worldBounds.maxX - halfWindow.x;
    const float minCameraY = worldBounds.minY + halfWindow.y;
    const float maxCameraY = worldBounds.maxY - halfWindow.y;

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

    m_CameraPosition.x = ClampToRangeOrCenter(
        m_PlayerSpawn.x, m_CameraBoundsMin.x, m_CameraBoundsMax.x);
    m_CameraPosition.y = ClampToRangeOrCenter(
        m_PlayerSpawn.y, m_CameraBoundsMin.y, m_CameraBoundsMax.y);
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