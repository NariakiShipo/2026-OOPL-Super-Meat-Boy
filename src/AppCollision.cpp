#include "App.hpp"

#include "game/Collision.hpp"

void App::ResolvePlayerPlatformCollisions(const glm::vec2 &previousPosition) {
    constexpr float wallBounceMinSpeed = 100.0F;
    constexpr float wallBounceDamping = 0.22F;

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

        const float incomingVelocityX = m_PlayerVelocity.x;
        if (std::abs(incomingVelocityX) > wallBounceMinSpeed) {
            m_PlayerVelocity.x = -incomingVelocityX * wallBounceDamping;
        } else {
            m_PlayerVelocity.x = 0.0F;
        }

        if (m_WallReattachCooldownMs <= 0.0F) {
            m_PlayerOnWall = true;
            m_WallJumpDirection = sideDirection;
        }
        playerAabb = makePlayerAabb();
    }
}
