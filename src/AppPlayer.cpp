#include "App.hpp"

#include "Util/Logger.hpp"

void App::ApplyPlayerDrawable(const std::shared_ptr<Core::Drawable> &drawable) {
    if (drawable != nullptr && m_Player != nullptr) {
        m_Player->SetDrawable(drawable);
    }
}

void App::UpdatePlayerAnimation(const float moveAxis) {
    if (moveAxis > 0.0F) {
        m_PlayerFacingRight = true;
    } else if (moveAxis < 0.0F) {
        m_PlayerFacingRight = false;
    }

    PlayerAnimState nextState = PlayerAnimState::IDLE;
    constexpr float airborneThreshold = 20.0F;
    constexpr float runThreshold = 1.0F;

    if (!m_PlayerOnGround) {
        if (m_PlayerVelocity.y > airborneThreshold) {
            nextState = PlayerAnimState::JUMP;
        } else {
            nextState = PlayerAnimState::FALL;
        }
    } else if (std::abs(m_PlayerVelocity.x) > runThreshold) {
        nextState = PlayerAnimState::RUN;
    }

    if (nextState == m_PlayerAnimState) {
        return;
    }

    m_PlayerAnimState = nextState;

    switch (m_PlayerAnimState) {
    case PlayerAnimState::IDLE:
        ApplyPlayerDrawable(m_PlayerIdleDrawable);
        break;
    case PlayerAnimState::RUN:
        if (m_PlayerFacingRight) {
            ApplyPlayerDrawable(m_PlayerRunRightDrawable);
        } else {
            ApplyPlayerDrawable(m_PlayerRunLeftDrawable);
        }
        break;
    case PlayerAnimState::JUMP:
        ApplyPlayerDrawable(m_PlayerJumpDrawable);
        break;
    case PlayerAnimState::FALL:
        ApplyPlayerDrawable(m_PlayerFallDrawable);
        break;
    }
}

void App::RespawnPlayer() {
    m_Player->m_Transform.translation = m_PlayerSpawn;
    m_PlayerVelocity = {0.0F, 0.0F};
    m_CameraLookaheadOffset = {0.0F, 0.0F};
    m_IsJumping = false;
    m_JumpHoldTimerMs = 0.0F;
    m_PlayerOnGround = false;
    m_PlayerOnWall = false;
    m_WallJumpDirection = 0.0F;
    m_WallControlLockTimerMs = 0.0F;
    m_WallReattachCooldownMs = 0.0F;
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerFacingRight = true;
    ApplyPlayerDrawable(m_PlayerIdleDrawable);
    m_LevelCleared = false;
    if (m_StatusText != nullptr) {
        m_StatusText->SetText("Reach the flag. Avoid red blocks.");
    }
    ++m_RespawnCount;
    LOG_INFO("Respawn count: {}", m_RespawnCount);
}
