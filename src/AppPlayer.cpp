#include "App.hpp"

#include "Util/Logger.hpp"

void App::ApplyPlayerDrawable(const std::shared_ptr<Core::Drawable> &drawable) {
    if (drawable != nullptr && m_Player != nullptr) {
        m_Player->SetDrawable(drawable);
    }
}

void App::UpdatePlayerAnimation(const float moveAxis) {
    PlayerAnimState nextState = PlayerAnimState::IDLE;
    const float airborneThreshold = m_Config.player.animation.airborneThreshold;
    const float runThreshold = m_Config.player.animation.runThreshold;

    // Prioritize actual movement direction, but also consider input direction
    // to handle direction changes that haven't updated velocity yet
    if (std::abs(m_PlayerVelocity.x) > runThreshold) {
        // Moving at non-trivial speed: trust velocity
        m_PlayerFacingRight = (m_PlayerVelocity.x > 0.0F);
    } else if (moveAxis > 0.0F) {
        // Not moving much: use input
        m_PlayerFacingRight = true;
    } else if (moveAxis < 0.0F) {
        m_PlayerFacingRight = false;
    }
    // If moveAxis == 0, keep current facing

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
        //m_StatusText->SetText("Reach the flag. Avoid red blocks.");
    }
    ++m_RespawnCount;
    LOG_INFO("Respawn count: {}", m_RespawnCount);
}

void App::UpdateBreakableBlocks() {
    for (std::size_t i = 0; i < m_BreakableBlocks.size(); ++i) {
        auto &breakable = m_BreakableBlocks[i];
        if (breakable.broken || !breakable.breaking || breakable.animation == nullptr ||
            breakable.object == nullptr) {
            continue;
        }

        if (breakable.animation->GetState() == Util::Animation::State::ENDED) {
            const auto blockPos = breakable.object->m_Transform.translation;
            breakable.object->SetVisible(false);
            breakable.breaking = false;
            breakable.broken = true;
            LOG_INFO("Breakable[{}] animation ended. Block removed at ({}, {})",
                     i, blockPos.x, blockPos.y);
        }
    }
}
