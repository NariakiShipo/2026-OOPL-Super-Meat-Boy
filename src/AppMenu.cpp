#include "App.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

#include "common/AssetPath.hpp"

#include "Util/Color.hpp"
#include "Util/BGM.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/SFX.hpp"
#include "Util/Text.hpp"
#include "Util/Time.hpp"
#include "Util/TransformUtils.hpp"

#include "config.hpp"
#include "game/Collision.hpp"

namespace {
std::shared_ptr<Util::Text> GetTextDrawable(
    const std::shared_ptr<Util::GameObject> &object) {
    if (object == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Util::Text>(object->GetDrawable());
}

// 對高 zIndex UI 物件的滑鼠碰撞體：
// PTSD 對 zIndex >= 100 的物件強制 zoom=1（UI 層），渲染位置 =
// translation - cameraPosition、尺寸不縮放，滑鼠判定需一致（勿乘 zoom）。
static Game::Aabb GetUiAabb(const std::shared_ptr<Util::GameObject> &object) {
    const glm::vec2 camPos = Util::GetCameraPosition();
    const glm::vec2 screenPos = object->m_Transform.translation - camPos;
    return Game::MakeAabb(screenPos, object->GetScaledSize());
}

bool IsCursorOver(const std::shared_ptr<Util::GameObject> &object) {
    if (object == nullptr || object->GetDrawable() == nullptr) {
        return false;
    }

    const auto cursor = Util::Input::GetCursorPosition();
    const Game::Aabb bounds = GetUiAabb(object);
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

std::shared_ptr<Util::GameObject> MakeText(const std::string &text,
                                           const int fontSize,
                                           const glm::vec2 &position,
                                           const float zIndex,
                                           const Util::Color &color) {
    auto object = std::make_shared<Util::GameObject>();
    object->SetDrawable(std::make_shared<Util::Text>(
        Common::ResolveAssetPath("fonts/Inter.ttf"), fontSize, text, color));
    object->m_Transform.translation = position;
    object->SetZIndex(zIndex);
    return object;
}

std::shared_ptr<Util::GameObject> MakeImage(const std::string &relativePath,
                                            const glm::vec2 &position,
                                            const glm::vec2 &size,
                                            const float zIndex) {
    auto object = std::make_shared<Util::GameObject>();
    auto image = std::make_shared<Util::Image>(Common::ResolveAssetPath(relativePath));
    object->SetDrawable(image);
    const auto textureSize = image->GetSize();
    if (textureSize.x > 0.0F && textureSize.y > 0.0F) {
        object->m_Transform.scale = {size.x / textureSize.x, size.y / textureSize.y};
    }
    object->m_Transform.translation = position;
    object->SetZIndex(zIndex);
    return object;
}

// 鍵盤選單導航：上下移動 index，回傳是否按下確認鍵
bool MenuNavigate(int *index, const int itemCount) {
    if (index == nullptr || itemCount <= 0) {
        return false;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::DOWN) ||
        Util::Input::IsKeyDown(Util::Keycode::S)) {
        *index = (*index + 1) % itemCount;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::UP) ||
        Util::Input::IsKeyDown(Util::Keycode::W)) {
        *index = (*index + itemCount - 1) % itemCount;
    }
    return Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
           Util::Input::IsKeyDown(Util::Keycode::RETURN);
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
    Util::SetCameraZoom(1.0F);
    Util::SetCameraPosition({0.0F, 0.0F});

    m_Root = Util::Renderer();
    SetVisibleObjects(m_SettingsObjects, false);
    m_SettingsMenuVisible = false;

    // 隱藏舊版滑鼠按鈕（保留物件以相容設定選單的開關邏輯）
    if (m_StartButton != nullptr) {
        m_StartButton->SetVisible(false);
    }
    if (m_SettingsButton != nullptr) {
        m_SettingsButton->SetVisible(false);
    }

    // 背景沿用 titlescreen
    if (m_TitleBackground != nullptr) {
        m_TitleBackground->SetVisible(true);
        m_Root.AddChild(m_TitleBackground);
    }

    // ── 階段1：PRESS START 黑色橫條 ──────────────────────────
    m_PressStartPanel = MakeImage("images/ui_panel.png", {0.0F, -240.0F},
                                  {700.0F, 100.0F}, 120.0F);
    m_PressStartText = MakeText("PRESS START", 44, {0.0F, -240.0F}, 130.0F,
                                Util::Color(255, 255, 255, 255));
    m_Root.AddChild(m_PressStartPanel);
    m_Root.AddChild(m_PressStartText);

    // ── 階段2：主選單（右側黑面板 + 箭頭游標）────────────────
    m_TitleMenuPanel = MakeImage("images/ui_panel.png", {300.0F, -60.0F},
                                 {560.0F, 380.0F}, 120.0F);
    m_TitleMenuItems.clear();
    const std::array<std::string, 4> labels = {"START GAME", "CHARACTERS",
                                               "SETTINGS", "EXIT GAME"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        m_TitleMenuItems.push_back(
            MakeText(labels[i], 38,
                     {330.0F, 40.0F - static_cast<float>(i) * 80.0F}, 130.0F,
                     Util::Color(255, 255, 255, 255)));
    }
    m_TitleMenuArrow = MakeImage("images/ui_arrow.png", {120.0F, 40.0F},
                                 {44.0F, 30.0F}, 130.0F);
    m_TitleHintText = MakeText("[SPACE] SELECT    [ESC] BACK", 20,
                               {300.0F, -240.0F}, 130.0F,
                               Util::Color(200, 200, 200, 255));
    m_Root.AddChild(m_TitleMenuPanel);
    for (const auto &item : m_TitleMenuItems) {
        m_Root.AddChild(item);
    }
    m_Root.AddChild(m_TitleMenuArrow);
    m_Root.AddChild(m_TitleHintText);

    for (const auto &object : m_SettingsObjects) {
        m_Root.AddChild(object);
    }
    for (const auto &object : m_CharacterSelectObjects) {
        m_Root.AddChild(object);
    }

    m_TitleMenuIndex = 0;
    m_TitleBlinkMs = 0.0F;
    SetTitlePhase(TitlePhase::PRESS_START);
    RefreshSettingsText();
}

// 切換標題畫面階段並設定對應可見性
void App::SetTitlePhase(const TitlePhase phase) {
    m_TitlePhase = phase;
    const bool menuVisible = (phase == TitlePhase::MENU);

    if (m_PressStartPanel != nullptr) {
        m_PressStartPanel->SetVisible(!menuVisible);
    }
    if (m_PressStartText != nullptr) {
        m_PressStartText->SetVisible(!menuVisible);
    }
    if (m_TitleMenuPanel != nullptr) {
        m_TitleMenuPanel->SetVisible(menuVisible);
    }
    for (const auto &item : m_TitleMenuItems) {
        if (item != nullptr) {
            item->SetVisible(menuVisible);
        }
    }
    if (m_TitleMenuArrow != nullptr) {
        m_TitleMenuArrow->SetVisible(menuVisible);
    }
    if (m_TitleHintText != nullptr) {
        m_TitleHintText->SetVisible(menuVisible);
    }
}

void App::ShowGameplayScreen() {
    m_Root = Util::Renderer();
    if (m_StartButton != nullptr) {
        m_StartButton->SetVisible(false);
    }
    if (m_SettingsButton != nullptr) {
        m_SettingsButton->SetVisible(false);
    }
    // 確保標題選單所有元素在遊戲中都不可見
    if (m_TitleBackground != nullptr) {
        m_TitleBackground->SetVisible(false);
    }
    if (m_PressStartPanel != nullptr) {
        m_PressStartPanel->SetVisible(false);
    }
    if (m_PressStartText != nullptr) {
        m_PressStartText->SetVisible(false);
    }
    if (m_TitleMenuPanel != nullptr) {
        m_TitleMenuPanel->SetVisible(false);
    }
    for (const auto &item : m_TitleMenuItems) {
        if (item != nullptr) {
            item->SetVisible(false);
        }
    }
    if (m_TitleMenuArrow != nullptr) {
        m_TitleMenuArrow->SetVisible(false);
    }
    if (m_TitleHintText != nullptr) {
        m_TitleHintText->SetVisible(false);
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
    for (const auto &rotor : m_Rotors) {
        if (rotor.bar != nullptr) {
            m_Root.AddChild(rotor.bar);
        }
        if (rotor.sawA != nullptr) {
            m_Root.AddChild(rotor.sawA);
        }
        if (rotor.sawB != nullptr) {
            m_Root.AddChild(rotor.sawB);
        }
    }
    if (m_Boss.object != nullptr) {
        m_Root.AddChild(m_Boss.object);
    }
    m_Root.AddChild(m_GoalFlag);
    for (const auto &bandage : m_Bandages) {
        if (bandage.object != nullptr) {
            m_Root.AddChild(bandage.object);
        }
    }
    m_Root.AddChild(m_Player);
    m_Root.AddChild(m_StatusBoard);
    if (m_TimerText != nullptr) {
        m_Root.AddChild(m_TimerText);
    }
    if (m_BandageCountText != nullptr) {
        m_Root.AddChild(m_BandageCountText);
    }
    if (m_CheatIndicator != nullptr) {
        m_Root.AddChild(m_CheatIndicator);
    }
    for (const auto &object : m_LevelCompleteObjects) {
        m_Root.AddChild(object);
    }
    for (const auto &object : m_SettingsObjects) {
        m_Root.AddChild(object);
    }
    for (const auto &object : m_CharacterSelectObjects) {
        m_Root.AddChild(object);
    }
    for (const auto &object : m_PauseObjects) {
        m_Root.AddChild(object);
    }
    SetVisibleObjects(m_PauseObjects, false);
    m_PauseMenuVisible = false;

    RefreshSettingsText();
}

void App::OpenSettingsMenu() {
    m_SettingsMenuVisible = true;

    // 設定頁為 zIndex>=100 的 UI 層（不受 zoom 影響），
    // 只需以相機位置為原點重新放置即可
    const glm::vec2 camPos = Util::GetCameraPosition();
    for (std::size_t i = 0; i < m_SettingsObjects.size() &&
                            i < m_SettingsBasePositions.size(); ++i) {
        if (m_SettingsObjects[i] == nullptr) {
            continue;
        }
        m_SettingsObjects[i]->m_Transform.translation =
            camPos + m_SettingsBasePositions[i];
        m_SettingsObjects[i]->m_Transform.scale = m_SettingsBaseScales[i];
    }

    SetVisibleObjects(m_SettingsObjects, true);
    if (m_CurrentState == State::TITLE) {
        // 設定開啟時隱藏標題選單元素
        SetTitlePhase(TitlePhase::MENU);
        if (m_TitleMenuPanel != nullptr) {
            m_TitleMenuPanel->SetVisible(true);  // 面板留作設定背景
        }
        for (const auto &item : m_TitleMenuItems) {
            if (item != nullptr) {
                item->SetVisible(false);
            }
        }
        if (m_TitleMenuArrow != nullptr) {
            m_TitleMenuArrow->SetVisible(false);
        }
        if (m_TitleHintText != nullptr) {
            m_TitleHintText->SetVisible(false);
        }
    }
    // 暫停選單開著時先藏起來（從暫停選單進入設定）
    if (m_CurrentState == State::UPDATE) {
        SetVisibleObjects(m_PauseObjects, false);
    }
    // 只在遊戲中（UPDATE state）才顯示「返回關卡選擇」按鈕
    if (m_SettingsToLevelSelectButton != nullptr) {
        m_SettingsToLevelSelectButton->SetVisible(
            m_CurrentState == State::UPDATE);
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
        SetTitlePhase(TitlePhase::MENU);  // 回主選單階段
    }
    // 從遊戲中的設定返回 → 回到暫停選單
    if (m_CurrentState == State::UPDATE && m_PauseMenuVisible) {
        SetVisibleObjects(m_PauseObjects, true);
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
    m_LevelSelectWorldIndex = 0;
    ShowWorldSelectScreen();
    m_CurrentState = State::WORLD_SELECT;
}

void App::UpdateTitleScreen() {
    // ── 階段1：PRESS START（文字閃爍）────────────────────────
    if (m_TitlePhase == TitlePhase::PRESS_START) {
        m_TitleBlinkMs += Util::Time::GetDeltaTimeMs();
        if (m_PressStartText != nullptr) {
            m_PressStartText->SetVisible(
                std::fmod(m_TitleBlinkMs, 1000.0F) < 650.0F);
        }
        if (Util::Input::IsKeyDown(Util::Keycode::RETURN) ||
            Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
            Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
            if (m_ButtonSFX != nullptr) {
                m_ButtonSFX->Play();
            }
            SetTitlePhase(TitlePhase::MENU);
        }
        return;
    }

    // ── 階段2：主選單（鍵盤導航 + 滑鼠 hover）────────────────
    const int itemCount = static_cast<int>(m_TitleMenuItems.size());
    bool confirm = MenuNavigate(&m_TitleMenuIndex, itemCount);

    for (int i = 0; i < itemCount; ++i) {
        if (IsCursorOver(m_TitleMenuItems[i])) {
            m_TitleMenuIndex = i;
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
                confirm = true;
            }
        }
        SetButtonColor(m_TitleMenuItems[i],
                       (i == m_TitleMenuIndex)
                           ? m_Config.ui.buttonHoverColor
                           : m_Config.ui.buttonDefaultColor);
    }

    if (m_TitleMenuArrow != nullptr && m_TitleMenuIndex < itemCount) {
        m_TitleMenuArrow->m_Transform.translation = {
            120.0F, m_TitleMenuItems[m_TitleMenuIndex]->m_Transform.translation.y};
    }

    if (confirm) {
        switch (m_TitleMenuIndex) {
        case 0: StartGame(); break;
        case 1: OpenCharacterSelect(); break;
        case 2: OpenSettingsMenu(); break;
        case 3: m_CurrentState = State::END; break;
        default: break;
        }
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::F1)) {
        OpenSettingsMenu();
    }
}

void App::UpdateSettingsMenu() {
    const auto bgmText = GetTextDrawable(m_BgmSettingText);
    if (bgmText != nullptr) {
        // 音量條碰撞使用螢幕座標（含相機位移與縮放）
        const Game::Aabb bgmBounds = GetUiAabb(m_BgmSettingText);
        const auto cursor = Util::Input::GetCursorPosition();
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
        // 音量條碰撞使用螢幕座標（含相機位移與縮放）
        const Game::Aabb sfxBounds = GetUiAabb(m_SfxSettingText);
        const auto cursor = Util::Input::GetCursorPosition();
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
        return;
    }

    // 「返回關卡選擇」按鈕：只在遊戲中（UPDATE state）顯示
    if (m_CurrentState == State::UPDATE &&
        m_SettingsToLevelSelectButton != nullptr) {
        SetButtonColor(m_SettingsToLevelSelectButton,
                       IsCursorOver(m_SettingsToLevelSelectButton)
                           ? m_Config.ui.buttonHoverColor
                           : Util::Color(255, 200, 200, 255));

        if (IsCursorOver(m_SettingsToLevelSelectButton) &&
            Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
            CloseSettingsMenu();
            m_LevelSelectWorldIndex = 0;
            ShowLevelSelectScreen();
            m_CurrentState = State::LEVEL_SELECT;
            return;
        }
    }
}

void App::Title() {
    if (m_CharacterSelectVisible &&
        (Util::Input::IsKeyDown(Util::Keycode::F1) ||
         Util::Input::IsKeyDown(Util::Keycode::ESCAPE))) {
        CloseCharacterSelect();
    } else if (m_CharacterSelectVisible) {
        UpdateCharacterSelect();
    } else if (m_SettingsMenuVisible &&
        (Util::Input::IsKeyDown(Util::Keycode::F1) ||
         Util::Input::IsKeyDown(Util::Keycode::ESCAPE))) {
        CloseSettingsMenu();
    } else if (m_SettingsMenuVisible) {
        UpdateSettingsMenu();
    } else if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
        if (m_TitlePhase == TitlePhase::MENU) {
            SetTitlePhase(TitlePhase::PRESS_START);  // 主選單 → 回 PRESS START
            m_TitleBlinkMs = 0.0F;
        } else {
            m_CurrentState = State::END;             // PRESS START → 離開遊戲
        }
    } else {
        UpdateTitleScreen();
    }

    m_Root.Update();

    if (Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

// ── 暫停選單（GAME PAUSED）──────────────────────────────────
namespace {
constexpr std::array<const char *, 6> kPauseLabels = {
    "RESUME GAME", "RESTART LEVEL", "EXIT TO MAP",
    "CHANGE CHARACTER", "SETTINGS", "EXIT GAME"};
constexpr float kPauseItemStartY = 110.0F;
constexpr float kPauseItemStepY = 75.0F;
} // namespace

void App::BuildPauseMenu() {
    m_PauseObjects.clear();
    m_PauseMenuItems.clear();

    auto overlay = MakeImage("images/ui_panel.png", {0.0F, 0.0F},
                             {1400.0F, 800.0F}, 200.0F);
    m_PauseObjects.push_back(overlay);

    auto header = MakeText("GAME PAUSED", 56, {0.0F, 240.0F}, 210.0F,
                           Util::Color(255, 255, 255, 255));
    m_PauseObjects.push_back(header);

    for (std::size_t i = 0; i < kPauseLabels.size(); ++i) {
        auto item = MakeText(kPauseLabels[i], 34,
                             {40.0F, kPauseItemStartY -
                                         static_cast<float>(i) * kPauseItemStepY},
                             210.0F, Util::Color(255, 255, 255, 255));
        m_PauseMenuItems.push_back(item);
        m_PauseObjects.push_back(item);
    }

    m_PauseArrow = MakeImage("images/ui_arrow.png", {-220.0F, kPauseItemStartY},
                             {44.0F, 30.0F}, 210.0F);
    m_PauseObjects.push_back(m_PauseArrow);

    auto hint = MakeText("[SPACE] SELECT    [ESC] RESUME", 20, {0.0F, -300.0F},
                         210.0F, Util::Color(200, 200, 200, 255));
    m_PauseObjects.push_back(hint);

    // 記錄基準 transform 供 OpenPauseMenu 依相機重新放置
    m_PauseBasePositions.clear();
    m_PauseBaseScales.clear();
    for (const auto &object : m_PauseObjects) {
        m_PauseBasePositions.push_back(
            object != nullptr ? object->m_Transform.translation
                              : glm::vec2{0.0F, 0.0F});
        m_PauseBaseScales.push_back(object != nullptr
                                        ? object->m_Transform.scale
                                        : glm::vec2{1.0F, 1.0F});
    }

    SetVisibleObjects(m_PauseObjects, false);
}

void App::OpenPauseMenu() {
    m_PauseMenuVisible = true;
    m_PauseMenuIndex = 0;

    // 暫停選單為 zIndex>=100 的 UI 層（PTSD 對其強制 zoom=1），
    // 只需以相機位置為原點放置，尺寸用基準值即可
    const glm::vec2 camPos = Util::GetCameraPosition();

    for (std::size_t i = 0; i < m_PauseObjects.size() &&
                            i < m_PauseBasePositions.size(); ++i) {
        if (m_PauseObjects[i] == nullptr) {
            continue;
        }
        m_PauseObjects[i]->m_Transform.translation =
            camPos + m_PauseBasePositions[i];
        m_PauseObjects[i]->m_Transform.scale = m_PauseBaseScales[i];
    }

    SetVisibleObjects(m_PauseObjects, true);
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::ClosePauseMenu() {
    m_PauseMenuVisible = false;
    SetVisibleObjects(m_PauseObjects, false);
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::UpdatePauseMenu() {
    const int itemCount = static_cast<int>(m_PauseMenuItems.size());
    bool confirm = MenuNavigate(&m_PauseMenuIndex, itemCount);

    for (int i = 0; i < itemCount; ++i) {
        if (IsCursorOver(m_PauseMenuItems[i])) {
            m_PauseMenuIndex = i;
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
                confirm = true;
            }
        }
        SetButtonColor(m_PauseMenuItems[i],
                       (i == m_PauseMenuIndex)
                           ? m_Config.ui.buttonHoverColor
                           : m_Config.ui.buttonDefaultColor);
    }

    if (m_PauseArrow != nullptr && m_PauseMenuIndex < itemCount) {
        const auto itemPos =
            m_PauseMenuItems[m_PauseMenuIndex]->m_Transform.translation;
        m_PauseArrow->m_Transform.translation = {itemPos.x - 260.0F,
                                                 itemPos.y};
    }

    if (!confirm) {
        return;
    }

    switch (m_PauseMenuIndex) {
    case 0:  // RESUME
        ClosePauseMenu();
        break;
    case 1:  // RESTART LEVEL
        ClosePauseMenu();
        RespawnPlayer();
        break;
    case 2:  // EXIT TO MAP
        ClosePauseMenu();
        ShowLevelSelectScreen();
        m_CurrentState = State::LEVEL_SELECT;
        break;
    case 3:  // CHANGE CHARACTER
        OpenCharacterSelect();
        break;
    case 4:  // SETTINGS
        OpenSettingsMenu();
        break;
    case 5:  // EXIT GAME
        m_CurrentState = State::END;
        break;
    default:
        break;
    }
}

// ── 角色選擇（CHANGE CHARACTER）────────────────────────────
namespace {
constexpr float kCharPreviewHeight = 300.0F;  // 預覽圖等比縮放的目標高度
}  // namespace

void App::BuildCharacterSelect() {
    m_CharacterSelectObjects.clear();
    m_CharacterPreviewImages.clear();

    // 預載每個角色的 idle 圖作為中央預覽
    for (const auto &ch : m_Characters) {
        m_CharacterPreviewImages.push_back(std::make_shared<Util::Image>(
            Common::ResolveAssetPath(ch.idleSpritePath)));
    }

    m_CharacterSelectObjects.push_back(
        MakeImage("images/ui_panel.png", {0.0F, 0.0F}, {1400.0F, 800.0F}, 200.0F));
    m_CharacterSelectObjects.push_back(
        MakeText("CHANGE CHARACTER", 48, {0.0F, 300.0F}, 210.0F,
                 Util::Color(255, 255, 255, 255)));

    // 左上角繃帶圖示 + 數量
    m_CharacterSelectObjects.push_back(MakeImage(
        "images/bandage.png", {-600.0F, 318.0F}, {40.0F, 40.0F}, 211.0F));
    m_CharacterBandageText = MakeText("X 0", 30, {-535.0F, 318.0F}, 211.0F,
                                      Util::Color(255, 255, 255, 255));
    m_CharacterSelectObjects.push_back(m_CharacterBandageText);

    // 中央大預覽（drawable / scale 於 RefreshCharacterSelect 設定）
    m_CharacterPreview = std::make_shared<Util::GameObject>();
    if (!m_CharacterPreviewImages.empty()) {
        m_CharacterPreview->SetDrawable(m_CharacterPreviewImages.front());
    }
    m_CharacterPreview->m_Transform.translation = {0.0F, 60.0F};
    m_CharacterPreview->SetZIndex(210.0F);
    m_CharacterSelectObjects.push_back(m_CharacterPreview);

    m_CharacterNameText = MakeText("", 40, {0.0F, -150.0F}, 211.0F,
                                   Util::Color(255, 210, 110, 255));
    m_CharacterSelectObjects.push_back(m_CharacterNameText);

    m_CharacterLockText = MakeText("", 22, {0.0F, -198.0F}, 212.0F,
                                   Util::Color(255, 120, 120, 255));
    m_CharacterSelectObjects.push_back(m_CharacterLockText);

    m_CharacterDescText = MakeText("", 22, {0.0F, -250.0F}, 211.0F,
                                   Util::Color(220, 220, 220, 255));
    m_CharacterSelectObjects.push_back(m_CharacterDescText);

    m_CharacterLeftArrow = MakeImage("images/ui_arrow.png", {-360.0F, 60.0F},
                                     {64.0F, 44.0F}, 211.0F);
    m_CharacterLeftArrow->m_Transform.scale.x *= -1.0F;  // 翻轉指向左
    m_CharacterSelectObjects.push_back(m_CharacterLeftArrow);

    m_CharacterRightArrow = MakeImage("images/ui_arrow.png", {360.0F, 60.0F},
                                      {64.0F, 44.0F}, 211.0F);
    m_CharacterSelectObjects.push_back(m_CharacterRightArrow);

    m_CharacterSelectObjects.push_back(
        MakeText("[A/D] BROWSE    [SPACE] SELECT    [ESC] BACK", 20,
                 {0.0F, -312.0F}, 211.0F, Util::Color(200, 200, 200, 255)));

    // 記錄基準 transform 供 OpenCharacterSelect 依相機重新放置
    m_CharacterSelectBasePositions.clear();
    m_CharacterSelectBaseScales.clear();
    for (const auto &object : m_CharacterSelectObjects) {
        m_CharacterSelectBasePositions.push_back(
            object != nullptr ? object->m_Transform.translation
                              : glm::vec2{0.0F, 0.0F});
        m_CharacterSelectBaseScales.push_back(
            object != nullptr ? object->m_Transform.scale : glm::vec2{1.0F, 1.0F});
    }

    SetVisibleObjects(m_CharacterSelectObjects, false);
}

void App::RefreshCharacterSelect() {
    const int count = static_cast<int>(m_Characters.size());
    if (count == 0) {
        return;
    }
    m_CharacterBrowseIndex = (m_CharacterBrowseIndex % count + count) % count;
    const int idx = m_CharacterBrowseIndex;
    const auto &ch = m_Characters[idx];
    const bool unlocked = IsCharacterUnlocked(idx);

    if (m_CharacterPreview != nullptr &&
        idx < static_cast<int>(m_CharacterPreviewImages.size()) &&
        m_CharacterPreviewImages[idx] != nullptr) {
        m_CharacterPreview->SetDrawable(m_CharacterPreviewImages[idx]);
        const auto size = m_CharacterPreviewImages[idx]->GetSize();
        if (size.y > 0.0F) {
            const float scale = kCharPreviewHeight / size.y;
            m_CharacterPreview->m_Transform.scale = {scale, scale};
        }
    }

    const auto name = GetTextDrawable(m_CharacterNameText);
    if (name != nullptr) {
        name->SetText(ch.name);
        name->SetColor(unlocked ? Util::Color(255, 210, 110, 255)
                                : Util::Color(150, 150, 150, 255));
    }
    const auto desc = GetTextDrawable(m_CharacterDescText);
    if (desc != nullptr) {
        desc->SetText(ch.description);
    }
    const auto lock = GetTextDrawable(m_CharacterLockText);
    if (lock != nullptr) {
        if (!unlocked) {
            lock->SetText("LOCKED - NEED " + std::to_string(ch.unlockCost) +
                          " BANDAGES (HAVE " +
                          std::to_string(m_BandagesCollected) + ")");
            lock->SetColor(Util::Color(255, 120, 120, 255));
        } else if (idx == m_SelectedCharacter) {
            lock->SetText("CURRENTLY SELECTED");
            lock->SetColor(Util::Color(140, 230, 140, 255));
        } else {
            lock->SetText("PRESS [SPACE] TO SELECT");
            lock->SetColor(Util::Color(255, 255, 255, 255));
        }
    }
    const auto bandage = GetTextDrawable(m_CharacterBandageText);
    if (bandage != nullptr) {
        bandage->SetText("X " + std::to_string(m_BandagesCollected));
    }
}

void App::OpenCharacterSelect() {
    m_CharacterSelectVisible = true;
    m_CharacterBrowseIndex = m_SelectedCharacter;

    const glm::vec2 camPos = Util::GetCameraPosition();
    for (std::size_t i = 0; i < m_CharacterSelectObjects.size() &&
                            i < m_CharacterSelectBasePositions.size(); ++i) {
        if (m_CharacterSelectObjects[i] == nullptr) {
            continue;
        }
        m_CharacterSelectObjects[i]->m_Transform.translation =
            camPos + m_CharacterSelectBasePositions[i];
        m_CharacterSelectObjects[i]->m_Transform.scale =
            m_CharacterSelectBaseScales[i];
    }
    SetVisibleObjects(m_CharacterSelectObjects, true);

    // 隱藏底下的選單，關閉時還原
    if (m_CurrentState == State::TITLE) {
        for (const auto &item : m_TitleMenuItems) {
            if (item != nullptr) {
                item->SetVisible(false);
            }
        }
        if (m_TitleMenuArrow != nullptr) {
            m_TitleMenuArrow->SetVisible(false);
        }
        if (m_TitleHintText != nullptr) {
            m_TitleHintText->SetVisible(false);
        }
    } else if (m_CurrentState == State::UPDATE) {
        SetVisibleObjects(m_PauseObjects, false);
    }

    RefreshCharacterSelect();
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::CloseCharacterSelect() {
    m_CharacterSelectVisible = false;
    SetVisibleObjects(m_CharacterSelectObjects, false);

    if (m_CurrentState == State::TITLE) {
        SetTitlePhase(TitlePhase::MENU);  // 還原標題主選單
    } else if (m_CurrentState == State::UPDATE && m_PauseMenuVisible) {
        SetVisibleObjects(m_PauseObjects, true);  // 還原暫停選單
    }
    if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
    }
}

void App::UpdateCharacterSelect() {
    const int count = static_cast<int>(m_Characters.size());
    if (count == 0) {
        return;
    }

    bool browseChanged = false;
    if (Util::Input::IsKeyDown(Util::Keycode::LEFT) ||
        Util::Input::IsKeyDown(Util::Keycode::A)) {
        --m_CharacterBrowseIndex;
        browseChanged = true;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::RIGHT) ||
        Util::Input::IsKeyDown(Util::Keycode::D)) {
        ++m_CharacterBrowseIndex;
        browseChanged = true;
    }

    bool selectPressed = Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
                         Util::Input::IsKeyDown(Util::Keycode::RETURN);

    if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        if (IsCursorOver(m_CharacterLeftArrow)) {
            --m_CharacterBrowseIndex;
            browseChanged = true;
        } else if (IsCursorOver(m_CharacterRightArrow)) {
            ++m_CharacterBrowseIndex;
            browseChanged = true;
        } else if (IsCursorOver(m_CharacterPreview)) {
            selectPressed = true;
        }
    }

    if (browseChanged) {
        m_CharacterBrowseIndex = (m_CharacterBrowseIndex % count + count) % count;
        RefreshCharacterSelect();
        if (m_ButtonSFX != nullptr) {
            m_ButtonSFX->Play();
        }
    }

    if (selectPressed && IsCharacterUnlocked(m_CharacterBrowseIndex)) {
        m_SelectedCharacter = m_CharacterBrowseIndex;
        ApplyCharacterToPlayer();
        SaveProgress();
        RefreshCharacterSelect();
        if (m_ButtonSFX != nullptr) {
            m_ButtonSFX->Play();
        }
    }
}
