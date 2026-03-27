#include "App.hpp"

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"
#include "Util/TransformUtils.hpp"

namespace {
struct Aabb {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

Aabb GetAabb(const std::shared_ptr<Util::GameObject> &obj) {
    const auto center = obj->m_Transform.translation;
    const auto half = obj->GetScaledSize() * 0.5F;
    return {
        center.x - half.x,
        center.x + half.x,
        center.y - half.y,
        center.y + half.y,
    };
}

bool IsOverlap(const Aabb &a, const Aabb &b) {
    return a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY &&
           a.maxY > b.minY;
}
} // namespace

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
    m_PlayerOnGround = false;
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
        const auto center = m_Player->m_Transform.translation;
        const auto half = m_PlayerColliderSize * 0.5F;
        return Aabb{center.x - half.x, center.x + half.x, center.y - half.y,
                    center.y + half.y};
    };

    auto playerAabb = makePlayerAabb();
    const auto playerHalf = m_PlayerColliderSize * 0.5F;

    for (const auto &platform : m_Platforms) {
        const auto platformAabb = GetAabb(platform);
        const auto platformPosition = platform->m_Transform.translation;
        const auto platformHalf = platform->GetScaledSize() * 0.5F;
        const auto currentPosition = m_Player->m_Transform.translation;

        if (!IsOverlap(playerAabb, platformAabb)) {
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

        if (m_Player->m_Transform.translation.x < platformPosition.x) {
            m_Player->m_Transform.translation.x -= overlapX;
        } else {
            m_Player->m_Transform.translation.x += overlapX;
        }
        m_PlayerVelocity.x = 0.0F;
        playerAabb = makePlayerAabb();
    }
}

void App::StepPlayer(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    constexpr float moveSpeed = 360.0F;
    // constexpr float jumpVelocity = 620.0F;
    constexpr float jumpVelocity = 1000.0F;
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
    m_PlayerVelocity.x = moveAxis * moveSpeed;

    const bool jumpPressed = Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
                             Util::Input::IsKeyDown(Util::Keycode::W) ||
                             Util::Input::IsKeyDown(Util::Keycode::UP);
    if (m_PlayerOnGround && jumpPressed) {
        m_PlayerVelocity.y = jumpVelocity;
        m_PlayerOnGround = false;
    }

    m_PlayerVelocity.y += gravity * dtSec;

    const auto previousPosition = m_Player->m_Transform.translation;
    m_Player->m_Transform.translation += m_PlayerVelocity * dtSec;

    m_PlayerOnGround = false;
    ResolvePlayerPlatformCollisions(previousPosition);
    UpdatePlayerAnimation(moveAxis);

    const auto playerCenter = m_Player->m_Transform.translation;
    const auto playerHalf = m_PlayerColliderSize * 0.5F;
    const Aabb playerAabb = {
        playerCenter.x - playerHalf.x,
        playerCenter.x + playerHalf.x,
        playerCenter.y - playerHalf.y,
        playerCenter.y + playerHalf.y,
    };
    for (const auto &deathZone : m_DeathZones) {
        if (IsOverlap(playerAabb, GetAabb(deathZone))) {
            LOG_INFO("Hit death zone. Respawning...");
            RespawnPlayer();
            return;
        }
    }

    if (m_GoalFlag != nullptr && IsOverlap(playerAabb, GetAabb(m_GoalFlag))) {
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