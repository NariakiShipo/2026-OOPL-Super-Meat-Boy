#include "App.hpp"

#include <cmath>

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

    // RUN 狀態還細分左右朝向：朝向改變時即使狀態仍是 RUN，也必須重貼對應方向的
    // 跑步圖，否則向右跑直接反向往左跑（速度一幀內跨過 runThreshold）會卡在右跑動畫。
    const bool runDirectionChanged =
        nextState == PlayerAnimState::RUN &&
        m_PlayerFacingRight != m_PlayerRunDrawableFacingRight;
    if (nextState == m_PlayerAnimState && !runDirectionChanged) {
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
        m_PlayerRunDrawableFacingRight = m_PlayerFacingRight;
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
    m_JumpBufferTimerMs = 0.0F;
    m_CoyoteTimerMs = 0.0F;
    m_PlayerOnGround = false;
    m_PlayerOnWall = false;
    m_WallJumpDirection = 0.0F;
    m_WallControlLockTimerMs = 0.0F;
    m_WallReattachCooldownMs = 0.0F;
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerFacingRight = true;
    m_PlayerRunDrawableFacingRight = true;
    ApplyPlayerDrawable(m_PlayerIdleDrawable);
    m_LevelCleared = false;
    m_LevelTimeMs = 0.0F;  // 每次重生（含死亡重試）重新計時
    HideLevelCompleteUI();  // 還原 HUD、隱藏通關畫面
    if (m_StatusText != nullptr) {
        //m_StatusText->SetText("Reach the flag. Avoid red blocks.");
    }
    ++m_RespawnCount;
    LOG_INFO("Respawn count: {}", m_RespawnCount);

    // 死亡 = 整張地圖狀態重置：Boss 回起點、破壞物復原、飛鋸清場
    ResetBoss();
    ResetLevelDynamics();
}

// 重置關卡動態狀態（死亡/重生時呼叫），使破壞物等回復原樣
void App::ResetLevelDynamics() {
    // 碎裂磚復原：動畫回第 0 幀、重新顯示、旗標歸零
    for (auto &breakable : m_BreakableBlocks) {
        breakable.breaking = false;
        breakable.broken = false;
        if (breakable.animation != nullptr) {
            breakable.animation->SetCurrentFrame(0);
            breakable.animation->Pause();
        }
        if (breakable.object != nullptr) {
            breakable.object->SetVisible(true);
        }
    }

    // 場上飛行中的鋸片全部清除
    for (auto &bz : m_LiveBuzzsaws) {
        if (bz.object != nullptr) {
            bz.object->SetVisible(false);
        }
        bz.active = false;
    }
    m_LiveBuzzsaws.clear();

    // 發射器計時重新倒數（重生後保留一個完整間隔的反應時間）
    for (auto &shooter : m_Shooters) {
        shooter.timerMs = m_Config.buzzsaw.intervalMs;
    }

    // 旋轉鋸回到起始角（每次重試的時機完全一致）
    for (auto &rotor : m_Rotors) {
        rotor.angleDeg = std::fmod(rotor.config.startAngleDeg, 360.0F);
        if (rotor.angleDeg < 0.0F) {
            rotor.angleDeg += 360.0F;
        }
    }
    UpdateRotors(0.0F);  // 立即套用位置/角度，避免殘留上一幀狀態

    ResetBandages();  // 繃帶復原：重新顯示、標記為未收集
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
