#include "App.hpp"
#include "Util/Input.hpp"
#include "Util/Time.hpp"

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