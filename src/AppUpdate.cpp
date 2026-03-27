#include "App.hpp"

#include "game/Collision.hpp"

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "Util/TransformUtils.hpp"

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
    m_IsJumping = false;
    m_JumpHoldTimerMs = 0.0F;
    m_PlayerOnGround = false;
    m_PlayerOnWall = false;
    m_WallJumpDirection = 0.0F;
    m_WallControlLockTimerMs = 0.0F;
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

void App::ResolvePlayerPlatformCollisions(const glm::vec2 &previousPosition) {
    const auto makePlayerAabb = [this]() {
        return Game::MakeAabb(m_Player->m_Transform.translation,
                              m_PlayerColliderSize);
    };

    auto playerAabb = makePlayerAabb();
    const auto playerHalf = m_PlayerColliderSize * 0.5F;

    for (const auto &platform : m_Platforms) {
        const auto platformAabb = Game::GetAabb(platform);
        const auto platformPosition = platform->m_Transform.translation;
        const auto platformHalf = platform->GetScaledSize() * 0.5F;
        const auto currentPosition = m_Player->m_Transform.translation;

        if (!Game::IsOverlap(playerAabb, platformAabb)) {
            continue;
        }

        const float platformTop = platformPosition.y + platformHalf.y;
        const float platformBottom = platformPosition.y - platformHalf.y;

        const float previousBottom = previousPosition.y - playerHalf.y;
        const float currentBottom = currentPosition.y - playerHalf.y;
        if (previousBottom >= platformTop && currentBottom < platformTop) {
            m_Player->m_Transform.translation.y = platformTop + playerHalf.y;
            m_PlayerVelocity.y = 0.0F;
            m_PlayerOnGround = true;
            playerAabb = makePlayerAabb();
            continue;
        }

        const float previousTop = previousPosition.y + playerHalf.y;
        const float currentTop = currentPosition.y + playerHalf.y;
        if (previousTop <= platformBottom && currentTop > platformBottom) {
            m_Player->m_Transform.translation.y = platformBottom - playerHalf.y;
            if (m_PlayerVelocity.y > 0.0F) {
                m_PlayerVelocity.y = 0.0F;
            }
            playerAabb = makePlayerAabb();
            continue;
        }

        const float overlapX =
            std::min(playerAabb.maxX, platformAabb.maxX) -
            std::max(playerAabb.minX, platformAabb.minX);
        if (overlapX <= 0.0F) {
            continue;
        }

        float sideDirection = 0.0F;
        if (m_Player->m_Transform.translation.x < platformPosition.x) {
            m_Player->m_Transform.translation.x -= overlapX;
            sideDirection = -1.0F;
        } else {
            m_Player->m_Transform.translation.x += overlapX;
            sideDirection = 1.0F;
        }
        m_PlayerVelocity.x = 0.0F;
        m_PlayerOnWall = true;
        m_WallJumpDirection = sideDirection;
        playerAabb = makePlayerAabb();
    }
}

void App::StepPlayer(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    constexpr float moveSpeed = 360.0F;
    constexpr float sprintMultiplier = 1.8F;
    constexpr float jumpVelocity = 1000.0F;
    constexpr float jumpHoldMaxMs = 170.0F;
    constexpr float jumpHoldBoost = 1500.0F;
    constexpr float shortHopCutRatio = 0.42F;
    constexpr float wallJumpHorizontalVelocity = 430.0F;
    constexpr float wallJumpControlLockMs = 140.0F;
    constexpr float wallSlideMaxFallSpeed = 260.0F;
    constexpr float gravity = -1800.0F;
    constexpr float killY = -520.0F;

    float moveAxis = 0.0F;
    if (Util::Input::IsKeyPressed(Util::Keycode::A) ||
        Util::Input::IsKeyPressed(Util::Keycode::LEFT)) {
        moveAxis -= 1.0F;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::D) ||
        Util::Input::IsKeyPressed(Util::Keycode::RIGHT)) {
        moveAxis += 1.0F;
    }

    const bool sprinting = Util::Input::IsKeyPressed(Util::Keycode::LSHIFT) ||
                           Util::Input::IsKeyPressed(Util::Keycode::RSHIFT);
    const float currentMoveSpeed = sprinting ? moveSpeed * sprintMultiplier : moveSpeed;
    if (m_WallControlLockTimerMs > 0.0F) {
        m_WallControlLockTimerMs = std::max(0.0F, m_WallControlLockTimerMs - dtMs);
    } else {
        m_PlayerVelocity.x = moveAxis * currentMoveSpeed;
    }

    const bool jumpPressed = Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
                             Util::Input::IsKeyDown(Util::Keycode::W) ||
                             Util::Input::IsKeyDown(Util::Keycode::UP);
    const bool jumpHeld = Util::Input::IsKeyPressed(Util::Keycode::SPACE) ||
                          Util::Input::IsKeyPressed(Util::Keycode::W) ||
                          Util::Input::IsKeyPressed(Util::Keycode::UP);

    const bool canWallJump = m_PlayerOnWall && !m_PlayerOnGround;
    if ((m_PlayerOnGround || canWallJump) && jumpPressed) {
        m_PlayerVelocity.y = jumpVelocity;
        if (canWallJump) {
            m_PlayerVelocity.x = wallJumpHorizontalVelocity * m_WallJumpDirection;
            m_WallControlLockTimerMs = wallJumpControlLockMs;
            m_PlayerOnWall = false;
            m_WallJumpDirection = 0.0F;
        }
        m_IsJumping = true;
        m_JumpHoldTimerMs = 0.0F;
        m_PlayerOnGround = false;
    }

    if (m_IsJumping && jumpHeld && m_JumpHoldTimerMs < jumpHoldMaxMs) {
        m_PlayerVelocity.y += jumpHoldBoost * dtSec;
        m_JumpHoldTimerMs += dtMs;
    }

    if (m_IsJumping && !jumpHeld && m_PlayerVelocity.y > 0.0F) {
        m_PlayerVelocity.y *= shortHopCutRatio;
        m_IsJumping = false;
    }

    m_PlayerVelocity.y += gravity * dtSec;

    const auto previousPosition = m_Player->m_Transform.translation;
    m_Player->m_Transform.translation += m_PlayerVelocity * dtSec;

    m_PlayerOnGround = false;
    m_PlayerOnWall = false;
    m_WallJumpDirection = 0.0F;
    ResolvePlayerPlatformCollisions(previousPosition);

    // Wall slide: cap downward speed while sticking to a platform side.
    if (!m_PlayerOnGround && m_PlayerOnWall &&
        m_PlayerVelocity.y < -wallSlideMaxFallSpeed) {
        m_PlayerVelocity.y = -wallSlideMaxFallSpeed;
        m_IsJumping = false;
    }

    if (m_PlayerOnGround || m_PlayerVelocity.y <= 0.0F) {
        m_IsJumping = false;
    }
    UpdatePlayerAnimation(moveAxis);

    const auto playerCenter = m_Player->m_Transform.translation;
    const auto playerAabb = Game::MakeAabb(playerCenter, m_PlayerColliderSize);
    for (const auto &deathZone : m_DeathZones) {
        if (Game::IsOverlap(playerAabb, Game::GetAabb(deathZone))) {
            LOG_INFO("Hit death zone. Respawning...");
            RespawnPlayer();
            return;
        }
    }

    if (m_GoalFlag != nullptr &&
        Game::IsOverlap(playerAabb, Game::GetAabb(m_GoalFlag))) {
        if (!m_LevelCleared) {
            m_LevelCleared = true;
            m_PlayerVelocity = {0.0F, 0.0F};
            m_PlayerAnimState = PlayerAnimState::IDLE;
            ApplyPlayerDrawable(m_PlayerIdleDrawable);
            if (m_StatusText != nullptr) {
                m_StatusText->SetText("LEVEL CLEAR! Press N for next level.");
            }
            LOG_INFO("Level clear!");
        }
    }

    if (m_Player->m_Transform.translation.y < killY ||
        Util::Input::IsKeyDown(Util::Keycode::R)) {
        RespawnPlayer();
    }
}

void App::UpdateCamera(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    const float targetX = m_Player->m_Transform.translation.x;
    const float smooth = std::clamp(6.0F * dtSec, 0.0F, 1.0F);
    m_CameraPosition.x = m_CameraPosition.x + (targetX - m_CameraPosition.x) * smooth;

    Util::SetCameraPosition(m_CameraPosition);

    // Keep status text anchored to the screen while world moves.
    if (m_StatusBoard != nullptr) {
        m_StatusBoard->m_Transform.translation = m_CameraPosition + glm::vec2{-500.0F, 300.0F};
    }
}

void App::Update() {
    if (m_LevelCleared) {
        if (Util::Input::IsKeyDown(Util::Keycode::N)) {
            LoadLevel((m_CurrentLevelIndex + 1) % m_Levels.size());
        }
    } else {
        const auto dtMs = std::max(Util::Time::GetDeltaTimeMs(), 1.0F);
        StepPlayer(dtMs);
        UpdateCamera(dtMs);
    }

    m_Root.Update();
    
    /*
     * Do not touch the code below as they serve the purpose for
     * closing the window.
     */
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
        Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}