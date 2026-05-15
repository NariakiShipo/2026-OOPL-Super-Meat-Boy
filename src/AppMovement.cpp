#include "App.hpp"

#include "game/Collision.hpp"

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"

void App::StepPlayer(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    constexpr float moveSpeed = 360.0F;
    constexpr float sprintMultiplier = 1.8F;
    constexpr float jumpVelocity = 500.0F;
    constexpr float jumpHoldMaxMs = 170.0F;
    constexpr float jumpHoldBoost = 2000.0F;
    constexpr float shortHopCutRatio = 0.42F;
    constexpr float wallJumpHorizontalVelocity = 430.0F;
    constexpr float wallJumpControlLockMs = 140.0F;
    constexpr float wallReattachCooldownMs = 110.0F;
    constexpr float wallSlideMaxFallSpeed = 260.0F;
    constexpr float gravity = -1800.0F;

    if (m_WallReattachCooldownMs > 0.0F) {
        m_WallReattachCooldownMs = std::max(0.0F, m_WallReattachCooldownMs - dtMs);
    }

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
            m_WallReattachCooldownMs = wallReattachCooldownMs;
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

    m_BreakableDebugLogCooldownMs =
        std::max(0.0F, m_BreakableDebugLogCooldownMs - dtMs);
    if (m_BreakableDebugLogCooldownMs <= 0.0F && !m_BreakableBlocks.empty()) {
        float nearestDistSq = std::numeric_limits<float>::max();
        bool foundValidBreakable = false;

        for (std::size_t i = 0; i < m_BreakableBlocks.size(); ++i) {
            const auto &breakable = m_BreakableBlocks[i];
            if (breakable.object == nullptr) {
                continue;
            }

            const auto blockPos = breakable.object->m_Transform.translation;
            const float dx = playerCenter.x - blockPos.x;
            const float dy = playerCenter.y - blockPos.y;
            const float distSq = (dx * dx) + (dy * dy);
            if (distSq < nearestDistSq) {
                nearestDistSq = distSq;
                foundValidBreakable = true;
            }
        }

        if (foundValidBreakable) {
            //const auto &nearest = m_BreakableBlocks[nearestIndex];
            //const auto blockPos = nearest.object->m_Transform.translation;
            //const bool overlapNow = Game::IsOverlap(playerAabb, Game::GetAabb(nearest.object));
        }

        m_BreakableDebugLogCooldownMs = 1000.0F;
    }

    if (m_GoalFlag != nullptr &&
        Game::IsOverlap(playerAabb, Game::GetAabb(m_GoalFlag))) {
        if (!m_LevelCleared) {
            m_LevelCleared = true;
            m_PlayerVelocity = {0.0F, 0.0F};
            m_PlayerAnimState = PlayerAnimState::IDLE;
            ApplyPlayerDrawable(m_PlayerIdleDrawable);
            if (m_StatusText != nullptr) {
               m_StatusText->SetText("LEVEL CLEAR! \nPress N for next level.");
            }
            LOG_INFO("Level clear!");
        }
    }

    const float outOfBoundsMinX = m_WorldBoundsMin.x - m_PlayerColliderSize.x;
    const float outOfBoundsMaxX = m_WorldBoundsMax.x + m_PlayerColliderSize.x;
    const float outOfBoundsMinY = m_WorldBoundsMin.y - m_PlayerColliderSize.y;
    const auto playerPosition = m_Player->m_Transform.translation;
    const bool isOutOfBounds = playerPosition.x < outOfBoundsMinX ||
                               playerPosition.x > outOfBoundsMaxX ||
                               playerPosition.y < outOfBoundsMinY;

    if (isOutOfBounds ||
        Util::Input::IsKeyDown(Util::Keycode::R)) {
        RespawnPlayer();
    }
}
