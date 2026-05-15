#include "App.hpp"

#include "game/Collision.hpp"

#include "Util/Logger.hpp"

void App::ResolvePlayerPlatformCollisions(const glm::vec2 &previousPosition) {
    const float wallBounceMinSpeed = m_Config.collision.wallBounceMinSpeed;
    const float wallBounceDamping = m_Config.collision.wallBounceDamping;
    const float breakableTopContactEpsilon =
        m_Config.collision.breakableTopContactEpsilon;

    const auto makePlayerAabb = [this]() {
        return Game::MakeAabb(m_Player->m_Transform.translation,
                              m_PlayerColliderSize);
    };

    auto playerAabb = makePlayerAabb();
    const auto playerHalf = m_PlayerColliderSize * 0.5F;

    const auto resolveAgainst = [&](const std::shared_ptr<Util::GameObject> &platform,
                                    bool *touchedTop = nullptr,
                                    const glm::vec2 *colliderSizeOverride = nullptr,
                                    const bool useRelaxedTopCheck = false) {
        const auto platformAabb = (colliderSizeOverride != nullptr)
                                      ? Game::MakeAabb(platform->m_Transform.translation,
                                                       *colliderSizeOverride)
                                      : Game::GetAabb(platform);
        const auto platformPosition = platform->m_Transform.translation;
        const auto platformHalf = (colliderSizeOverride != nullptr)
                                      ? (*colliderSizeOverride) * 0.5F
                                      : platform->GetScaledSize() * 0.5F;
        const auto currentPosition = m_Player->m_Transform.translation;

        if (!Game::IsOverlap(playerAabb, platformAabb)) {
            return;
        }

        const float platformTop = platformPosition.y + platformHalf.y;
        const float platformBottom = platformPosition.y - platformHalf.y;

        const float previousBottom = previousPosition.y - playerHalf.y;
        const float currentBottom = currentPosition.y - playerHalf.y;
        const bool touchedFromTop =
            useRelaxedTopCheck
                     ? (previousBottom >= (platformTop - breakableTopContactEpsilon) &&
                         currentBottom <= (platformTop + breakableTopContactEpsilon))
                : (previousBottom >= platformTop && currentBottom < platformTop);

        if (touchedFromTop) {
            m_Player->m_Transform.translation.y = platformTop + playerHalf.y;
            m_PlayerVelocity.y = 0.0F;
            m_PlayerOnGround = true;
            if (touchedTop != nullptr) {
                *touchedTop = true;
            }
            playerAabb = makePlayerAabb();
            return;
        }

        const float previousTop = previousPosition.y + playerHalf.y;
        const float currentTop = currentPosition.y + playerHalf.y;
        if (previousTop <= platformBottom && currentTop > platformBottom) {
            m_Player->m_Transform.translation.y = platformBottom - playerHalf.y;
            if (m_PlayerVelocity.y > 0.0F) {
                m_PlayerVelocity.y = 0.0F;
            }
            playerAabb = makePlayerAabb();
            return;
        }

        const float overlapX =
            std::min(playerAabb.maxX, platformAabb.maxX) -
            std::max(playerAabb.minX, platformAabb.minX);
        if (overlapX <= 0.0F) {
            return;
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
    };

    for (const auto &platform : m_Platforms) {
        resolveAgainst(platform);
    }

    for (std::size_t i = 0; i < m_BreakableBlocks.size(); ++i) {
        auto &breakable = m_BreakableBlocks[i];
        if (breakable.broken || breakable.object == nullptr) {
            continue;
        }

        bool touchedTop = false;
        resolveAgainst(breakable.object, &touchedTop, &breakable.colliderSize, true);
        if (touchedTop && !breakable.breaking) {
            breakable.breaking = true;
            if (breakable.animation != nullptr) {
                breakable.animation->Play();
            }

            const auto blockPos = breakable.object->m_Transform.translation;
            LOG_INFO(
                "Breakable[{}] triggered from top contact. player=({}, {}), block=({}, {})",
                i,
                m_Player->m_Transform.translation.x,
                m_Player->m_Transform.translation.y,
                blockPos.x,
                blockPos.y);
        }
    }
}
