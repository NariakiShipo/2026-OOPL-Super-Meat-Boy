#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include "Core/Drawable.hpp"

#include "game/LevelData.hpp"

#include "Util/GameObject.hpp"
#include "Util/Animation.hpp"
#include "Util/Renderer.hpp"
#include "Util/Text.hpp"

class App {
public:
    enum class PlayerAnimState {
        IDLE,
        RUN,
        JUMP,
        FALL,
    };

    enum class State {
        START,
        UPDATE,
        END,
    };

    State GetCurrentState() const { return m_CurrentState; }

    void Start();

    void Update();

    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    void InitWorld();
    void LoadLevel(std::size_t levelIndex);
    void StepPlayer(float dtMs);
    void UpdateCamera(float dtMs);
    void ResolvePlayerPlatformCollisions(const glm::vec2 &previousPosition);
    void UpdatePlayerAnimation(float moveAxis);
    void UpdateBreakableBlocks();
    void ApplyPlayerDrawable(const std::shared_ptr<Core::Drawable> &drawable);
    void RespawnPlayer();
    std::shared_ptr<Util::GameObject> CreatePlatform(const glm::vec2 &position,
                                                     const glm::vec2 &size,
                                                     float zIndex,
                                                     const std::string &texturePath,
                                                     const glm::vec4 &uvRect =
                                                         {0.0F, 0.0F, 1.0F, 1.0F},
                                                     bool visible = true) const;

private:
    struct BreakableBlock {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Animation> animation;
        glm::vec2 colliderSize = {0.0F, 0.0F};
        bool breaking = false;
        bool broken = false;
    };

    State m_CurrentState = State::START;

    Util::Renderer m_Root;

    std::shared_ptr<Util::GameObject> m_Player;
    std::shared_ptr<Core::Drawable> m_PlayerIdleDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunLeftDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunRightDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerJumpDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerFallDrawable;

    PlayerAnimState m_PlayerAnimState = PlayerAnimState::IDLE;
    bool m_PlayerFacingRight = true;

    std::vector<std::shared_ptr<Util::GameObject>> m_Platforms;
    std::vector<BreakableBlock> m_BreakableBlocks;
    std::vector<std::shared_ptr<Util::GameObject>> m_DeathZones;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelRenderTiles;
    std::shared_ptr<Util::GameObject> m_GoalFlag;

    std::shared_ptr<Util::GameObject> m_StatusBoard;
    std::shared_ptr<Util::Text> m_StatusText;

    std::vector<Game::LevelConfig> m_Levels;
    std::size_t m_CurrentLevelIndex = 0;

    glm::vec2 m_CameraPosition = {0.0F, 0.0F};
    glm::vec2 m_CameraBoundsMin = {0.0F, 0.0F};
    glm::vec2 m_CameraBoundsMax = {0.0F, 0.0F};
    glm::vec2 m_CameraLookaheadOffset = {0.0F, 0.0F};
    float m_CameraZoom = 1.0F;

    float m_CameraDeadZoneHalfWidth = 150.0F;
    float m_CameraDeadZoneHalfHeight = 90.0F;
    float m_CameraFollowSpeed = 6.0F;
    float m_CameraLookaheadTime = 0.22F;
    float m_CameraLookaheadResponse = 8.0F;
    float m_CameraLookaheadMaxX = 220.0F;
    float m_CameraLookaheadMaxUp = 110.0F;
    float m_CameraLookaheadMaxDown = 260.0F;

    glm::vec2 m_PlayerSpawn = {-520.0F, -40.0F};
    glm::vec2 m_WorldBoundsMin = {-640.0F, -480.0F};
    glm::vec2 m_WorldBoundsMax = {640.0F, 480.0F};
    glm::vec2 m_PlayerColliderSize = {96.0F, 96.0F};
    glm::vec2 m_PlayerVelocity = {0.0F, 0.0F};
    bool m_IsJumping = false;
    float m_JumpHoldTimerMs = 0.0F;
    bool m_PlayerOnGround = false;
    bool m_PlayerOnWall = false;
    float m_WallJumpDirection = 0.0F;
    float m_WallControlLockTimerMs = 0.0F;
    float m_WallReattachCooldownMs = 0.0F;
    float m_BreakableDebugLogCooldownMs = 0.0F;
    bool m_LevelCleared = false;
    int m_RespawnCount = 0;
};

#endif
