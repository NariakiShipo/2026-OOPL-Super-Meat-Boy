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
    const std::array<std::string, 3> labels = {"START GAME", "SETTINGS",
                                               "EXIT GAME"};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        m_TitleMenuItems.push_back(
            MakeText(labels[i], 38,
                     {330.0F, 40.0F - static_cast<float>(i) * 80.0F}, 130.0F,
                     Util::Color(255, 255, 255, 255)));
    }
    m_TitleMenuArrow = MakeImage("images/ui_arrow.png", {120.0F, 40.0F},
                                 {44.0F, 30.0F}, 130.0F);
    m_TitleHintText = MakeText("[SPACE] SELECT    [ESC] BACK", 20,
                               {300.0F, -210.0F}, 130.0F,
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
    m_Root.AddChild(m_Player);
    m_Root.AddChild(m_StatusBoard);
    if (m_CheatIndicator != nullptr) {
        m_Root.AddChild(m_CheatIndicator);
    }
    for (const auto &object : m_SettingsObjects) {
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
        case 1: OpenSettingsMenu(); break;
        case 2: m_CurrentState = State::END; break;
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
    if (m_SettingsMenuVisible &&
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
constexpr std::array<const char *, 5> kPauseLabels = {
    "RESUME GAME", "RESTART LEVEL", "EXIT TO MAP", "SETTINGS", "EXIT GAME"};
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
    case 3:  // SETTINGS
        OpenSettingsMenu();
        break;
    case 4:  // EXIT GAME
        m_CurrentState = State::END;
        break;
    default:
        break;
    }
}
