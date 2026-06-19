#include "App.hpp"

#include "game/Collision.hpp"

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"

void App::StepPlayer(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    const auto &movement = m_Config.player.movement;

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
    const float currentMoveSpeed =
        sprinting ? movement.moveSpeed * movement.sprintMultiplier
                  : movement.moveSpeed;

    // ── 水平：加速度模型（取代瞬間到速）──────────────────────
    // 有輸入 → 朝目標速度加速；無輸入 → 摩擦減速（地面滑行短、空中保留動量）
    if (m_WallControlLockTimerMs > 0.0F) {
        m_WallControlLockTimerMs = std::max(0.0F, m_WallControlLockTimerMs - dtMs);
    } else {
        const float targetSpeed = moveAxis * currentMoveSpeed;
        const bool hasInput = (moveAxis != 0.0F);
        const float accel =
            m_PlayerOnGround
                ? (hasInput ? movement.groundAccel : movement.groundDecel)
                : (hasInput ? movement.airAccel : movement.airDecel);
        const float maxStep = accel * dtSec;
        const float deltaV = targetSpeed - m_PlayerVelocity.x;
        m_PlayerVelocity.x += std::clamp(deltaV, -maxStep, maxStep);
    }

    const bool jumpPressed = Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
                             Util::Input::IsKeyDown(Util::Keycode::W) ||
                             Util::Input::IsKeyDown(Util::Keycode::UP);
    const bool jumpHeld = Util::Input::IsKeyPressed(Util::Keycode::SPACE) ||
                          Util::Input::IsKeyPressed(Util::Keycode::W) ||
                          Util::Input::IsKeyPressed(Util::Keycode::UP);

    // ── 跳躍容錯：input buffer + coyote time ─────────────────
    if (jumpPressed) {
        m_JumpBufferTimerMs = movement.jumpBufferMs;
    } else {
        m_JumpBufferTimerMs = std::max(0.0F, m_JumpBufferTimerMs - dtMs);
    }
    if (m_PlayerOnGround) {
        m_CoyoteTimerMs = movement.coyoteMs;
    } else {
        m_CoyoteTimerMs = std::max(0.0F, m_CoyoteTimerMs - dtMs);
    }

    const bool wantJump = m_JumpBufferTimerMs > 0.0F;
    const bool canGroundJump = m_PlayerOnGround || m_CoyoteTimerMs > 0.0F;
    const bool canWallJump = m_PlayerOnWall && !m_PlayerOnGround;
    if ((canGroundJump || canWallJump) && wantJump) {
        m_PlayerVelocity.y = movement.jumpVelocity;
        if (canWallJump && !canGroundJump) {
            m_PlayerVelocity.x =
                movement.wallJumpHorizontalVelocity * m_WallJumpDirection;
            m_WallControlLockTimerMs = movement.wallJumpControlLockMs;
            m_WallReattachCooldownMs = movement.wallReattachCooldownMs;
            m_PlayerOnWall = false;
            m_WallJumpDirection = 0.0F;
        }
        m_IsJumping = true;
        m_JumpHoldTimerMs = 0.0F;
        m_PlayerOnGround = false;
        m_JumpBufferTimerMs = 0.0F;  // 消耗緩衝
        m_CoyoteTimerMs = 0.0F;      // 消耗土狼時間
    }

    if (m_IsJumping && jumpHeld && m_JumpHoldTimerMs < movement.jumpHoldMaxMs) {
        m_PlayerVelocity.y += movement.jumpHoldBoost * dtSec;
        m_JumpHoldTimerMs += dtMs;
    }

    if (m_IsJumping && !jumpHeld && m_PlayerVelocity.y > 0.0F) {
        m_PlayerVelocity.y *= movement.shortHopCutRatio;
        m_IsJumping = false;
    }

    // ── 非對稱重力 + 終端速度（下落較快、落地乾脆）────────────
    const float gravityNow = (m_PlayerVelocity.y > 0.0F) ? movement.gravityUp
                                                         : movement.gravityDown;
    m_PlayerVelocity.y += gravityNow * dtSec;
    if (m_PlayerVelocity.y < -movement.maxFallSpeed) {
        m_PlayerVelocity.y = -movement.maxFallSpeed;
    }

    const auto previousPosition = m_Player->m_Transform.translation;
    m_Player->m_Transform.translation += m_PlayerVelocity * dtSec;

    m_PlayerOnGround = false;
    m_PlayerOnWall = false;
    m_WallJumpDirection = 0.0F;
    ResolvePlayerPlatformCollisions(previousPosition);

    // Wall slide: cap downward speed while sticking to a platform side.
    if (!m_PlayerOnGround && m_PlayerOnWall &&
        m_PlayerVelocity.y < -movement.wallSlideMaxFallSpeed) {
        m_PlayerVelocity.y = -movement.wallSlideMaxFallSpeed;
        m_IsJumping = false;
    }

    if (m_PlayerOnGround || m_PlayerVelocity.y <= 0.0F) {
        m_IsJumping = false;
    }
    UpdatePlayerAnimation(moveAxis);

    const auto playerCenter = m_Player->m_Transform.translation;
    const auto playerAabb = Game::MakeAabb(playerCenter, m_PlayerColliderSize);
    if (!m_CheatSawImmune) {
        for (const auto &deathZone : m_DeathZones) {
            if (Game::IsOverlap(playerAabb, Game::GetAabb(deathZone))) {
                LOG_INFO("Hit death zone. Respawning...");
                RespawnPlayer();
                return;
            }
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
                    m_StatusText->SetText(m_Config.ui.levelClearText);
            }
            BankLevelBandages();  // 本關已收集的繃帶計入全域總數
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
