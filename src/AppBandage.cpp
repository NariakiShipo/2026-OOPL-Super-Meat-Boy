#include "App.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "game/Collision.hpp"

#include "Util/Logger.hpp"
#include "Util/SFX.hpp"
#include "Util/Text.hpp"

int App::CollectedBandageCount() const {
    return static_cast<int>(std::count_if(
        m_Bandages.begin(), m_Bandages.end(),
        [](const Bandage &b) { return b.collected; }));
}

// 依 TMX 解析出的位置生成本關所有繃帶（每關重建）。
void App::SpawnBandages(const std::vector<glm::vec2> &positions) {
    m_Bandages.clear();
    m_Bandages.reserve(positions.size());
    for (const auto &pos : positions) {
        Bandage bandage{};
        bandage.object = CreatePlatform(pos, m_Config.bandage.size,
                                        m_Config.bandage.zIndex,
                                        m_Config.bandage.texturePath);
        bandage.colliderSize = m_Config.bandage.size;
        bandage.collected = false;
        m_Bandages.push_back(std::move(bandage));
    }
}

// 玩家碰到未收集的繃帶 → 收集（隱藏 + 音效）。過關後不再收集。
void App::CheckBandageCollection() {
    if (m_Bandages.empty() || m_Player == nullptr || m_LevelCleared) {
        return;
    }

    const auto playerAabb =
        Game::MakeAabb(m_Player->m_Transform.translation, m_PlayerColliderSize);

    for (auto &bandage : m_Bandages) {
        if (bandage.collected || bandage.object == nullptr) {
            continue;
        }
        const auto bandageAabb = Game::MakeAabb(
            bandage.object->m_Transform.translation, bandage.colliderSize);
        if (Game::IsOverlap(playerAabb, bandageAabb)) {
            bandage.collected = true;
            bandage.object->SetVisible(false);
            if (m_ButtonSFX != nullptr) {
                m_ButtonSFX->Play();  // 暫用按鈕音效，可日後換成專屬收集音
            }
            LOG_INFO("Bandage collected ({}/{})", CollectedBandageCount(),
                     m_Bandages.size());
        }
    }
}

// 死亡重生：整關繃帶復原（重新顯示、未收集），讓每次嘗試都能重新收集。
void App::ResetBandages() {
    for (auto &bandage : m_Bandages) {
        bandage.collected = false;
        if (bandage.object != nullptr) {
            bandage.object->SetVisible(true);
        }
    }
}

// 過關：把本關已收集的繃帶計入跨關卡全域總數。
void App::BankLevelBandages() {
    const int collected = CollectedBandageCount();
    m_BandagesCollected += collected;
    SaveProgress();  // 繃帶總數寫入存檔（角色解鎖門檻依此判定）
    LOG_INFO("Banked {} bandages this level (global total {})", collected,
             m_BandagesCollected);
}

// 每幀更新 HUD：計時器（秒）與本關繃帶數量（已收集/總數）。
void App::UpdateHud() {
    if (m_TimerText != nullptr) {
        const auto timer =
            std::dynamic_pointer_cast<Util::Text>(m_TimerText->GetDrawable());
        if (timer != nullptr) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.2f",
                          m_LevelTimeMs / 1000.0F);
            timer->SetText(m_Config.ui.timerPrefix + buffer);
        }
    }

    if (m_BandageCountText != nullptr) {
        const auto counter = std::dynamic_pointer_cast<Util::Text>(
            m_BandageCountText->GetDrawable());
        if (counter != nullptr) {
            counter->SetText(m_Config.ui.bandagePrefix +
                             std::to_string(CollectedBandageCount()) + "/" +
                             std::to_string(m_Bandages.size()));
        }
    }
}
