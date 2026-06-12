#include "App.hpp"

#include <algorithm>

#include "common/AssetPath.hpp"
#include "game/Collision.hpp"

#include "Util/Animation.hpp"
#include "Util/Logger.hpp"

// W1 Boss「Lil' Slugger」：鏈鋸步行機甲追擊戰。
// 狀態機：WAITING(進關延遲) → CHASING(等速向右) → RUSHING(玩家越過
// rushTriggerX 後加速) → CRASHED(抵達 crashX 撞牆停止、不再判死)。
// 死亡（任何原因）→ RespawnPlayer() 內呼叫 ResetBoss() 整段重來。

namespace {
std::vector<std::string> ResolveFramePaths(const std::vector<std::string> &framePaths) {
    std::vector<std::string> resolvedPaths;
    resolvedPaths.reserve(framePaths.size());
    for (const auto &framePath : framePaths) {
        const bool hasAbsolutePath =
            !framePath.empty() &&
            (framePath.front() == '/' || framePath.find(':') != std::string::npos);
        resolvedPaths.push_back(
            hasAbsolutePath ? framePath : Common::ResolveAssetPath(framePath));
    }
    return resolvedPaths;
}
} // namespace

void App::SpawnBoss() {
    m_Boss = BossState{};

    if (!m_LevelHasBoss) {
        return;
    }

    auto object = std::make_shared<Util::GameObject>();

    const std::size_t safeInterval =
        std::max<std::size_t>(m_Config.boss.animIntervalMs, 1);

    const auto walkPaths = ResolveFramePaths(m_Config.boss.animFrames);
    if (!walkPaths.empty()) {
        auto walkAnim = std::make_shared<Util::Animation>(
            walkPaths, true, safeInterval, true, 0);
        m_Boss.walkDrawable = walkAnim;
        object->SetDrawable(walkAnim);

        const auto &visualSize = m_Config.boss.visualSize;
        if (visualSize.x > 0.0F && visualSize.y > 0.0F) {
            const auto frameSize = walkAnim->GetSize();
            object->m_Transform.scale = {
                visualSize.x / frameSize.x,
                visualSize.y / frameSize.y,
            };
        }
    }

    // 撞毀燃燒 drawable（畫布尺寸與行走幀相同 → 共用 scale）
    const auto crashPaths = ResolveFramePaths(m_Config.boss.crashFrames);
    if (!crashPaths.empty()) {
        m_Boss.crashDrawable = std::make_shared<Util::Animation>(
            crashPaths, true, safeInterval, true, 0);
    }

    object->m_Transform.translation = m_BossSpawnPoint;
    object->SetZIndex(m_Config.boss.zIndex);
    object->SetVisible(true);

    m_Boss.object = object;
    ResetBoss();

    LOG_INFO("Boss spawned at ({:.1f}, {:.1f}), rushTriggerX={:.1f}, crashX={:.1f}",
             m_BossSpawnPoint.x, m_BossSpawnPoint.y,
             m_BossRushTriggerX, m_BossCrashX);
}

void App::ResetBoss() {
    if (m_Boss.object == nullptr) {
        return;
    }
    m_Boss.object->m_Transform.translation = m_BossSpawnPoint;
    m_Boss.phase = BossPhase::WAITING;
    m_Boss.waitTimerMs = m_Config.boss.startDelayMs;
    if (m_Boss.walkDrawable != nullptr) {
        m_Boss.object->SetDrawable(m_Boss.walkDrawable);
    }
    m_Boss.object->SetVisible(true);
}

void App::StepBoss(const float dtMs) {
    if (m_Boss.object == nullptr || m_Boss.phase == BossPhase::INACTIVE) {
        return;
    }

    const float dtSec = dtMs / 1000.0F;
    auto &position = m_Boss.object->m_Transform.translation;

    switch (m_Boss.phase) {
    case BossPhase::WAITING:
        m_Boss.waitTimerMs -= dtMs;
        if (m_Boss.waitTimerMs <= 0.0F) {
            m_Boss.phase = BossPhase::CHASING;
            LOG_INFO("Boss starts chasing!");
        }
        break;

    case BossPhase::CHASING:
        position.x += m_Config.boss.speed * dtSec;
        if (m_Player != nullptr &&
            m_Player->m_Transform.translation.x > m_BossRushTriggerX) {
            m_Boss.phase = BossPhase::RUSHING;
            LOG_INFO("Boss rushing toward crash wall!");
        }
        break;

    case BossPhase::RUSHING:
        position.x += m_Config.boss.rushSpeed * dtSec;
        break;

    case BossPhase::CRASHED:
    case BossPhase::INACTIVE:
        break;
    }

    // CHASING / RUSHING 抵達撞牆點 → 翻覆停止、換撞毀燃燒圖
    if ((m_Boss.phase == BossPhase::CHASING || m_Boss.phase == BossPhase::RUSHING) &&
        position.x >= m_BossCrashX) {
        position.x = m_BossCrashX;
        m_Boss.phase = BossPhase::CRASHED;
        if (m_Boss.crashDrawable != nullptr) {
            m_Boss.object->SetDrawable(m_Boss.crashDrawable);
        }
        LOG_INFO("Boss crashed at x={:.1f}", m_BossCrashX);
    }
}

void App::CheckBossPlayerCollision() {
    if (m_Boss.object == nullptr || m_Player == nullptr) {
        return;
    }
    // 只有追擊中才有殺傷力；WAITING（玩家碰不到）與 CRASHED（已撞毀）不判死
    if (m_Boss.phase != BossPhase::CHASING && m_Boss.phase != BossPhase::RUSHING) {
        return;
    }

    const auto playerAabb =
        Game::MakeAabb(m_Player->m_Transform.translation, m_PlayerColliderSize);

    // 雙 AABB：機身 + 前伸鏈鋸（鏈鋸又長又扁，單一大框會誤判）
    const auto center = m_Boss.object->m_Transform.translation;
    const auto bodyAabb =
        Game::MakeAabb(center + m_Config.boss.bodyOffset, m_Config.boss.bodySize);
    const auto sawAabb =
        Game::MakeAabb(center + m_Config.boss.sawOffset, m_Config.boss.sawSize);

    if (Game::IsOverlap(playerAabb, bodyAabb) ||
        Game::IsOverlap(playerAabb, sawAabb)) {
        LOG_INFO("Player caught by boss at ({:.1f}, {:.1f})", center.x, center.y);
        RespawnPlayer();  // RespawnPlayer 內會 ResetBoss()
        return;
    }

    // 防繞後：玩家跳高 (~235px) 足以越過 Boss，若整個落在機身後方
    // 即視為被輾過（原作中被 Boss 超過 = 死）
    if (playerAabb.maxX < bodyAabb.minX - m_Config.boss.behindKillMarginPx) {
        LOG_INFO("Player fell behind the boss. Caught!");
        RespawnPlayer();
    }
}
