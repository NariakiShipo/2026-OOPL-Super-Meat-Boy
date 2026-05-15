#include "App.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "Util/Color.hpp"
#include "Util/BGM.hpp"
#include "Util/Input.hpp"
#include "Util/SFX.hpp"
#include "Util/Text.hpp"

#include "game/Collision.hpp"

namespace {
std::shared_ptr<Util::Text> GetTextDrawable(
    const std::shared_ptr<Util::GameObject> &object) {
    if (object == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Util::Text>(object->GetDrawable());
}

bool IsCursorOver(const std::shared_ptr<Util::GameObject> &object) {
    if (object == nullptr || object->GetDrawable() == nullptr) {
        return false;
    }

    const auto cursor = Util::Input::GetCursorPosition();
    const Game::Aabb bounds = Game::GetAabb(object);
    return cursor.x >= bounds.minX && cursor.x <= bounds.maxX &&
           cursor.y >= bounds.minY && cursor.y <= bounds.maxY;
}

void SetButtonColor(const std::shared_ptr<Util::GameObject> &button,
                    const Util::Color &color) {
    const auto text = GetTextDrawable(button);
    if (text != nullptr) {
        text->SetColor(color);
    }
}

std::string BuildVolumeBar(const std::string &label, const int volume,
                           const int maxVolume, const int sliderBarWidth) {
    const int clampedVolume = std::clamp(volume, 0, maxVolume);
    const int filledCount =
        std::clamp((clampedVolume * sliderBarWidth + maxVolume / 2) /
                       maxVolume,
                   0, sliderBarWidth);
    return label + " [" + std::string(filledCount, '#') +
           std::string(sliderBarWidth - filledCount, '-') + "] " +
           std::to_string(clampedVolume);
}

int VolumeFromCursor(const Game::Aabb &bounds, const float cursorX,
                      const int maxVolume) {
    if (bounds.maxX <= bounds.minX) {
        return 0;
    }

    const float ratio = std::clamp((cursorX - bounds.minX) /
                                       (bounds.maxX - bounds.minX),
                                   0.0F, 1.0F);
    return static_cast<int>(std::round(ratio * static_cast<float>(maxVolume)));
}

void SetVisibleObjects(
    const std::vector<std::shared_ptr<Util::GameObject>> &objects,
    const bool visible) {
    for (const auto &object : objects) {
        if (object != nullptr) {
            object->SetVisible(visible);
        }
    }
}

} // namespace

void App::LoadAudioSettings() {
    std::ifstream input(m_AudioSettingsPath);

    if (!input.is_open()) {
        return;
    }

    int bgmVolume = m_AudioSettings.bgmVolume;
    int sfxVolume = m_AudioSettings.sfxVolume;
    if (input >> bgmVolume >> sfxVolume) {
        m_AudioSettings.bgmVolume =
            std::clamp(bgmVolume, 0, m_Config.audio.maxVolume);
        m_AudioSettings.sfxVolume =
            std::clamp(sfxVolume, 0, m_Config.audio.maxVolume);
    }
}

void App::SaveAudioSettings() const {
    std::ofstream output(m_AudioSettingsPath, std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << m_AudioSettings.bgmVolume << ' ' << m_AudioSettings.sfxVolume
           << '\n';
}

void App::ApplyAudioSettings() {
    m_AudioSettings.bgmVolume =
        std::clamp(m_AudioSettings.bgmVolume, 0, m_Config.audio.maxVolume);
    m_AudioSettings.sfxVolume =
        std::clamp(m_AudioSettings.sfxVolume, 0, m_Config.audio.maxVolume);

    if (m_TitleBGM != nullptr) {
        m_TitleBGM->SetVolume(m_AudioSettings.bgmVolume);
    }
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->SetVolume(m_AudioSettings.sfxVolume);
    }
}

void App::RefreshSettingsText() {
    const auto bgmText = GetTextDrawable(m_BgmSettingText);
    if (bgmText != nullptr) {
        bgmText->SetText(BuildVolumeBar(
            m_Config.ui.bgmLabel.text,
            m_AudioSettings.bgmVolume,
            m_Config.audio.maxVolume,
            m_Config.audio.sliderBarWidth));
    }

    const auto sfxText = GetTextDrawable(m_SfxSettingText);
    if (sfxText != nullptr) {
        sfxText->SetText(BuildVolumeBar(
            m_Config.ui.sfxLabel.text,
            m_AudioSettings.sfxVolume,
            m_Config.audio.maxVolume,
            m_Config.audio.sliderBarWidth));
    }
}

void App::ShowTitleScreen() {
    m_Root = Util::Renderer();
    SetVisibleObjects(m_TitleScreenObjects, true);
    if (m_StartButton != nullptr) {
        m_StartButton->SetVisible(true);
    }
    if (m_SettingsButton != nullptr) {
        m_SettingsButton->SetVisible(true);
    }
    SetVisibleObjects(m_SettingsObjects, false);
    m_SettingsMenuVisible = false;

    for (const auto &object : m_TitleScreenObjects) {
        m_Root.AddChild(object);
    }
    for (const auto &object : m_SettingsObjects) {
        m_Root.AddChild(object);
    }

    RefreshSettingsText();
}

void App::ShowGameplayScreen() {
    m_Root = Util::Renderer();
    if (m_StartButton != nullptr) {
        m_StartButton->SetVisible(false);
    }
    if (m_SettingsButton != nullptr) {
        m_SettingsButton->SetVisible(false);
    }
    SetVisibleObjects(m_SettingsObjects, false);
    m_SettingsMenuVisible = false;

    for (const auto &tile : m_LevelRenderTiles) {
        m_Root.AddChild(tile);
    }
    for (const auto &platform : m_Platforms) {
        m_Root.AddChild(platform);
    }
    for (const auto &breakable : m_BreakableBlocks) {
        m_Root.AddChild(breakable.object);
    }
    for (const auto &deathZone : m_DeathZones) {
        m_Root.AddChild(deathZone);
    }
    m_Root.AddChild(m_GoalFlag);
    m_Root.AddChild(m_Player);
    m_Root.AddChild(m_StatusBoard);
    for (const auto &object : m_SettingsObjects) {
        m_Root.AddChild(object);
    }

    RefreshSettingsText();
}

void App::OpenSettingsMenu() {
    m_SettingsMenuVisible = true;
    SetVisibleObjects(m_SettingsObjects, true);
    if (m_CurrentState == State::TITLE) {
        if (m_StartButton != nullptr) {
            m_StartButton->SetVisible(false);
        }
        if (m_SettingsButton != nullptr) {
            m_SettingsButton->SetVisible(false);
        }
    }
    RefreshSettingsText();
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::CloseSettingsMenu() {
    m_SettingsMenuVisible = false;
    SetVisibleObjects(m_SettingsObjects, false);
    if (m_CurrentState == State::TITLE) {
        if (m_StartButton != nullptr) {
            m_StartButton->SetVisible(true);
        }
        if (m_SettingsButton != nullptr) {
            m_SettingsButton->SetVisible(true);
        }
    }
    SaveAudioSettings();
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::StartGame() {
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }

    m_SettingsMenuVisible = false;
    LoadLevel(m_CurrentLevelIndex);
    m_CurrentState = State::UPDATE;
}

void App::UpdateTitleScreen() {
    const bool startHovered = IsCursorOver(m_StartButton);
    const bool settingsHovered = IsCursorOver(m_SettingsButton);

    SetButtonColor(m_StartButton,
                   startHovered ? m_Config.ui.buttonHoverColor
                                : m_Config.ui.buttonDefaultColor);
    SetButtonColor(m_SettingsButton,
                   settingsHovered ? m_Config.ui.buttonHoverColor
                                    : m_Config.ui.buttonDefaultColor);

    if (startHovered && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        StartGame();
        return;
    }

    if (settingsHovered && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        OpenSettingsMenu();
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::RETURN)) {
        StartGame();
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::F1)) {
        OpenSettingsMenu();
        return;
    }
}

void App::UpdateSettingsMenu() {
    const auto cursor = Util::Input::GetCursorPosition();

    const auto bgmText = GetTextDrawable(m_BgmSettingText);
    if (bgmText != nullptr) {
        const auto bgmBounds = Game::GetAabb(m_BgmSettingText);
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB) &&
            cursor.x >= bgmBounds.minX && cursor.x <= bgmBounds.maxX &&
            cursor.y >= bgmBounds.minY && cursor.y <= bgmBounds.maxY) {
            const int newVolume =
                VolumeFromCursor(bgmBounds, cursor.x, m_Config.audio.maxVolume);
            if (newVolume != m_AudioSettings.bgmVolume) {
                m_AudioSettings.bgmVolume = newVolume;
                ApplyAudioSettings();
                SaveAudioSettings();
                RefreshSettingsText();
            }
        }
    }

    const auto sfxText = GetTextDrawable(m_SfxSettingText);
    if (sfxText != nullptr) {
        const auto sfxBounds = Game::GetAabb(m_SfxSettingText);
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB) &&
            cursor.x >= sfxBounds.minX && cursor.x <= sfxBounds.maxX &&
            cursor.y >= sfxBounds.minY && cursor.y <= sfxBounds.maxY) {
            const int newVolume =
                VolumeFromCursor(sfxBounds, cursor.x, m_Config.audio.maxVolume);
            if (newVolume != m_AudioSettings.sfxVolume) {
                m_AudioSettings.sfxVolume = newVolume;
                ApplyAudioSettings();
                SaveAudioSettings();
                RefreshSettingsText();
            }
        }
    }

    SetButtonColor(m_SettingsBackButton,
                   IsCursorOver(m_SettingsBackButton) ? m_Config.ui.buttonHoverColor
                                                      : m_Config.ui.buttonDefaultColor);

    if (IsCursorOver(m_SettingsBackButton) &&
        Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        CloseSettingsMenu();
    }
}

void App::Title() {
    if (m_SettingsMenuVisible && Util::Input::IsKeyDown(Util::Keycode::F1)) {
        CloseSettingsMenu();
    }

    if (m_SettingsMenuVisible) {
        UpdateSettingsMenu();
    } else {
        UpdateTitleScreen();
    }

    m_Root.Update();

    if (!m_SettingsMenuVisible &&
        (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
         Util::Input::IfExit())) {
        m_CurrentState = State::END;
    }
}
