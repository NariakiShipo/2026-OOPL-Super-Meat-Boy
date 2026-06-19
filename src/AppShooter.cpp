#include "App.hpp"

#include <algorithm>

#include "common/AssetPath.hpp"
#include "game/Collision.hpp"

#include "Util/Animation.hpp"
#include "Util/Logger.hpp"

void App::UpdateShooters(const float dtMs) {
    const float speed       = m_Config.buzzsaw.speed;
    const float intervalMs  = m_Config.buzzsaw.intervalMs;
    const float bzZIndex    = m_Config.buzzsaw.zIndex;
    const auto &animFrames  = m_Config.buzzsaw.animFrames;
    const std::size_t animIntervalMs = m_Config.buzzsaw.animIntervalMs;

    // Resolve animation frame paths once
    std::vector<std::string> resolvedPaths;
    resolvedPaths.reserve(animFrames.size());
    for (const auto &framePath : animFrames) {
        const bool isAbsolute =
            !framePath.empty() &&
            (framePath.front() == '/' || framePath.find(':') != std::string::npos);
        resolvedPaths.push_back(
            isAbsolute ? framePath : Common::ResolveAssetPath(framePath));
    }

    // Tick each shooter and fire when timer expires
    for (auto &shooter : m_Shooters) {
        shooter.timerMs -= dtMs;
        if (shooter.timerMs > 0.0F) {
            continue;
        }

        // Reset timer (preserve sub-frame remainder to avoid drift)
        shooter.timerMs += intervalMs;

        // Determine velocity direction
        glm::vec2 velocity = {0.0F, 0.0F};
        switch (shooter.config.direction) {
        case Game::ShootDirection::Right: velocity = { speed,   0.0F  }; break;
        case Game::ShootDirection::Left:  velocity = {-speed,   0.0F  }; break;
        case Game::ShootDirection::Up:    velocity = { 0.0F,    speed }; break;
        case Game::ShootDirection::Down:  velocity = { 0.0F,   -speed }; break;
        }

        // Spawn buzzsaw GameObject with animation
        auto hazard = std::make_shared<Util::GameObject>();

        if (!resolvedPaths.empty()) {
            const std::size_t safeInterval = std::max<std::size_t>(animIntervalMs, 1);
            auto anim = std::make_shared<Util::Animation>(
                resolvedPaths, true, safeInterval, true, 0);
            hazard->SetDrawable(anim);

            const auto &cfgSize = m_Config.buzzsaw.size;
            if (cfgSize.x > 0.0F && cfgSize.y > 0.0F) {
                // Scale to explicit pixel size
                const auto frameSize = anim->GetSize();
                hazard->m_Transform.scale = {
                    cfgSize.x / frameSize.x,
                    cfgSize.y / frameSize.y,
                };
            } else {
                // Use natural (1:1) pixel size
                hazard->m_Transform.scale = {1.0F, 1.0F};
            }
        }

        hazard->m_Transform.translation = shooter.config.position;
        hazard->SetZIndex(bzZIndex);
        hazard->SetVisible(true);

        m_Root.AddChild(hazard);

        m_LiveBuzzsaws.push_back({hazard, velocity, m_Config.buzzsaw.spawnGraceMs, true});
        LOG_DEBUG("Buzzsaw spawned at ({:.1f}, {:.1f}) vel=({:.1f}, {:.1f})",
                  shooter.config.position.x, shooter.config.position.y,
                  velocity.x, velocity.y);
    }

    // Move all live buzzsaws; deactivate on world-bounds exit or platform hit
    const float dtSec = dtMs / 1000.0F;
    for (auto &bz : m_LiveBuzzsaws) {
        if (!bz.active) {
            continue;
        }

        bz.object->m_Transform.translation += bz.velocity * dtSec;

        // Tick spawn grace period
        if (bz.graceTimerMs > 0.0F) {
            bz.graceTimerMs -= dtMs;
        }

        const auto pos = bz.object->m_Transform.translation;

        // 1. Out of world bounds → deactivate
        if (pos.x < m_WorldBoundsMin.x || pos.x > m_WorldBoundsMax.x ||
            pos.y < m_WorldBoundsMin.y || pos.y > m_WorldBoundsMax.y) {
            bz.active = false;
            bz.object->SetVisible(false);
            continue;
        }

        // 2. Hit a platform — only after grace period expires
        if (bz.graceTimerMs > 0.0F) {
            continue;
        }
        const auto bzAabb = Game::GetAabb(bz.object);
        bool hitWall = false;

        for (const auto &platform : m_Platforms) {
            if (platform == nullptr) { continue; }
            if (Game::IsOverlap(bzAabb, Game::GetAabb(platform))) {
                hitWall = true;
                break;
            }
        }

        if (!hitWall) {
            for (const auto &breakable : m_BreakableBlocks) {
                if (breakable.broken || breakable.object == nullptr) { continue; }
                const auto bbAabb = Game::MakeAabb(
                    breakable.object->m_Transform.translation, breakable.colliderSize);
                if (Game::IsOverlap(bzAabb, bbAabb)) {
                    hitWall = true;
                    break;
                }
            }
        }

        if (hitWall) {
            bz.active = false;
            bz.object->SetVisible(false);
        }
    }

    // Remove inactive buzzsaws
    m_LiveBuzzsaws.erase(
        std::remove_if(m_LiveBuzzsaws.begin(), m_LiveBuzzsaws.end(),
                       [](const LiveBuzzsaw &bz) { return !bz.active; }),
        m_LiveBuzzsaws.end());
}

void App::CheckBuzzsawPlayerCollisions() {
    if (m_CheatSawImmune || m_LiveBuzzsaws.empty() || m_Player == nullptr) {
        return;
    }

    const auto playerAabb =
        Game::MakeAabb(m_Player->m_Transform.translation, m_PlayerColliderSize);

    for (const auto &bz : m_LiveBuzzsaws) {
        if (!bz.active || bz.object == nullptr) {
            continue;
        }

        // Use natural drawable size for buzzsaw AABB
        const auto bzAabb = Game::GetAabb(bz.object);
        if (Game::IsOverlap(playerAabb, bzAabb)) {
            LOG_INFO("Player hit by buzzsaw at ({:.1f}, {:.1f})",
                     bz.object->m_Transform.translation.x,
                     bz.object->m_Transform.translation.y);
            RespawnPlayer();
            return; // One death per frame is enough
        }
    }
}
