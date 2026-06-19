#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"

void App::Update() {
    // 作弊模式切換：F2 開關「碰到鏈鋸不會死亡」。放在最前面，暫停/遊戲中都可切換。
    if (Util::Input::IsKeyDown(Util::Keycode::F2)) {
        m_CheatSawImmune = !m_CheatSawImmune;
        LOG_INFO("Cheat [saw immune] = {}", m_CheatSawImmune ? "ON" : "OFF");
        if (m_CheatIndicator != nullptr) {
            m_CheatIndicator->SetVisible(m_CheatSawImmune);
        }
    }

    bool shouldUpdateGameplay = true;

    // 選單層級：設定（最上）→ 暫停 → 遊戲
    if (m_SettingsMenuVisible) {
        if (Util::Input::IsKeyDown(Util::Keycode::F1) ||
            Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
            CloseSettingsMenu();  // 關閉設定 → 回到暫停選單
        } else {
            UpdateSettingsMenu();
        }
        shouldUpdateGameplay = false;
    } else if (m_PauseMenuVisible) {
        if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
            ClosePauseMenu();
        } else {
            UpdatePauseMenu();
        }
        shouldUpdateGameplay = false;
    } else if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE) ||
               Util::Input::IsKeyDown(Util::Keycode::F1)) {
        OpenPauseMenu();
        shouldUpdateGameplay = false;
    } else if (m_LevelCleared) {
        if (Util::Input::IsKeyDown(Util::Keycode::N)) {
            LoadLevel((m_CurrentLevelIndex + 1) % m_Levels.size());
        }
    } else {
        const auto dtMs = std::max(Util::Time::GetDeltaTimeMs(), 1.0F);
        m_LevelTimeMs += dtMs;  // 關卡計時（過關/暫停時不在此分支，自動停止）
        StepPlayer(dtMs);
        CheckBandageCollection();
        UpdateShooters(dtMs);
        CheckBuzzsawPlayerCollisions();
        UpdateRotors(dtMs);
        CheckRotorPlayerCollisions();
        StepBoss(dtMs);
        CheckBossPlayerCollision();
        UpdateCamera(dtMs);
        UpdateHud();
    }

    m_Root.Update();
    if (shouldUpdateGameplay) {
        UpdateBreakableBlocks();
    }

    /*
     * Do not touch the code below as they serve the purpose for
     * closing the window.
     */
    if (Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}