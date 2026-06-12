#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Time.hpp"

void App::Update() {
    bool shouldUpdateGameplay = true;

    if (m_SettingsMenuVisible &&
        (Util::Input::IsKeyDown(Util::Keycode::F1) ||
         Util::Input::IsKeyDown(Util::Keycode::ESCAPE))) {
        CloseSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (m_SettingsMenuVisible) {
        UpdateSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (Util::Input::IsKeyDown(Util::Keycode::F1) ||
               Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
        OpenSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (m_LevelCleared) {
        if (Util::Input::IsKeyDown(Util::Keycode::N)) {
            LoadLevel((m_CurrentLevelIndex + 1) % m_Levels.size());
        }
    } else {
        const auto dtMs = std::max(Util::Time::GetDeltaTimeMs(), 1.0F);
        StepPlayer(dtMs);
        UpdateShooters(dtMs);
        CheckBuzzsawPlayerCollisions();
        UpdateRotors(dtMs);
        CheckRotorPlayerCollisions();
        StepBoss(dtMs);
        CheckBossPlayerCollision();
        UpdateCamera(dtMs);
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