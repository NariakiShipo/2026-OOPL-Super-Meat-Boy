#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include "Core/Drawable.hpp"

#include "Util/GameObject.hpp"
#include "Util/Renderer.hpp"
#include "Util/Text.hpp"

class App {
public:
    struct LevelObjectConfig {
        glm::vec2 position;
        glm::vec2 size;
        float zIndex;
        std::string texturePath;
    };

    struct LevelConfig {
        glm::vec2 spawn;
        glm::vec2 goalPosition;
        glm::vec2 goalSize;
        std::string goalTexturePath;
        std::vector<LevelObjectConfig> platforms;
        std::vector<LevelObjectConfig> deathZones;
    };

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
    void ApplyPlayerDrawable(const std::shared_ptr<Core::Drawable> &drawable);
    void RespawnPlayer();
    std::shared_ptr<Util::GameObject> CreatePlatform(const glm::vec2 &position,
                                                     const glm::vec2 &size,
                                                     float zIndex,
                                                     const std::string &texturePath) const;

private:
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
    std::vector<std::shared_ptr<Util::GameObject>> m_DeathZones;
    std::shared_ptr<Util::GameObject> m_GoalFlag;

    std::shared_ptr<Util::GameObject> m_StatusBoard;
    std::shared_ptr<Util::Text> m_StatusText;

    std::vector<LevelConfig> m_Levels;
    std::size_t m_CurrentLevelIndex = 0;

    glm::vec2 m_CameraPosition = {0.0F, 0.0F};

    glm::vec2 m_PlayerSpawn = {-520.0F, -40.0F};
    glm::vec2 m_PlayerColliderSize = {96.0F, 96.0F};
    glm::vec2 m_PlayerVelocity = {0.0F, 0.0F};
    bool m_PlayerOnGround = false;
    bool m_LevelCleared = false;
    int m_RespawnCount = 0;
};

#endif
