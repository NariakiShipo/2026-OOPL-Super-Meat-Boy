#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Time.hpp"

void App::Update() {
    bool shouldUpdateGameplay = true;

    if (m_SettingsMenuVisible && Util::Input::IsKeyDown(Util::Keycode::F1)) {
        CloseSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (m_SettingsMenuVisible) {
        UpdateSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (Util::Input::IsKeyDown(Util::Keycode::F1)) {
        OpenSettingsMenu();
        shouldUpdateGameplay = false;
    } else if (m_LevelCleared) {
        if (Util::Input::IsKeyDown(Util::Keycode::N)) {
            LoadLevel((m_CurrentLevelIndex + 1) % m_Levels.size());
        }
    } else {
        const auto dtMs = std::max(Util::Time::GetDeltaTimeMs(), 1.0F);
        StepPlayer(dtMs);
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
    if (!m_SettingsMenuVisible &&
        (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
         Util::Input::IfExit())) {
        m_CurrentState = State::END;
    }
}