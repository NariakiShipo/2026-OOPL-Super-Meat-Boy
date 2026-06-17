#include "App.hpp"

#include <cctype>
#include <string>

#include "common/AssetPath.hpp"

#include "Util/Color.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/SFX.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

#include "config.hpp"

// 世界地圖選擇（CH1: THE FOREST / CH2: THE FACTORY ...）
// 左右切換世界、SPACE/ENTER 進入關卡節點圖、ESC 回標題主選單。

namespace {
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

std::string UpperCase(std::string text) {
    for (auto &ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}
} // namespace

void App::ShowWorldSelectScreen() {
    Util::SetCameraZoom(1.0F);
    Util::SetCameraPosition({0.0F, 0.0F});

    m_Root = Util::Renderer();
    m_WorldSelectObjects.clear();

    if (m_Worlds.empty()) {
        return;
    }
    m_LevelSelectWorldIndex =
        m_LevelSelectWorldIndex % m_Worlds.size();
    const auto &world = m_Worlds[m_LevelSelectWorldIndex];

    // 背景：世界預覽圖滿版置底，前面壓暗色框營造聚焦
    m_WorldSelPreview = MakeImage(world.bgImagePath, {0.0F, -30.0F},
                                  {760.0F, 470.0F}, 0.0F);
    auto previewFrame = MakeImage("images/ui_panel_light.png", {0.0F, -30.0F},
                                  {784.0F, 494.0F}, -1.0F);
    m_WorldSelectObjects.push_back(previewFrame);
    m_WorldSelectObjects.push_back(m_WorldSelPreview);

    // 上方橫幅：CH#: NAME + 關卡數
    auto banner = MakeImage("images/ui_panel_light.png", {0.0F, 280.0F},
                            {1000.0F, 110.0F}, 100.0F);
    m_WorldSelectObjects.push_back(banner);

    const std::string chTitle =
        "CH" + std::to_string(m_LevelSelectWorldIndex + 1) + ": THE " +
        UpperCase(world.name);
    // Forest 含 BOSS 節點
    const bool hasBoss = (world.category == Game::WorldCategory::Forest) &&
                         (m_BossTestLevelIndex < m_Levels.size());
    const std::size_t nodeCount = world.levels.size() + (hasBoss ? 1 : 0);

    m_WorldSelTitleText = MakeText(chTitle, 40, {0.0F, 295.0F}, 110.0F,
                                   Util::Color(40, 40, 40, 255));
    m_WorldSelCountText = MakeText(
        "LEVELS: " + std::to_string(nodeCount) + (hasBoss ? "  (INCL. BOSS)" : ""),
        22, {0.0F, 252.0F}, 110.0F, Util::Color(90, 90, 90, 255));
    m_WorldSelectObjects.push_back(m_WorldSelTitleText);
    m_WorldSelectObjects.push_back(m_WorldSelCountText);

    // 左右切換指示
    if (m_Worlds.size() > 1) {
        m_WorldSelectObjects.push_back(MakeText("<", 64, {-460.0F, -30.0F},
                                                110.0F,
                                                Util::Color(255, 255, 255, 255)));
        m_WorldSelectObjects.push_back(MakeText(">", 64, {460.0F, -30.0F},
                                                110.0F,
                                                Util::Color(255, 255, 255, 255)));
    }

    // 底部操作提示
    auto hintBar = MakeImage("images/ui_panel.png", {0.0F, -320.0F},
                             {1300.0F, 70.0F}, 100.0F);
    m_WorldSelectObjects.push_back(hintBar);
    m_WorldSelectObjects.push_back(
        MakeText("[SPACE] SELECT", 24, {-470.0F, -320.0F}, 110.0F,
                 Util::Color(255, 255, 255, 255)));
    m_WorldSelectObjects.push_back(
        MakeText("[<] [>] SWITCH WORLD", 24, {0.0F, -320.0F}, 110.0F,
                 Util::Color(255, 255, 255, 255)));
    m_WorldSelectObjects.push_back(
        MakeText("[ESC] BACK", 24, {470.0F, -320.0F}, 110.0F,
                 Util::Color(255, 255, 255, 255)));

    for (const auto &object : m_WorldSelectObjects) {
        m_Root.AddChild(object);
    }
}

void App::UpdateWorldSelectScreen() {
    if (m_Worlds.empty()) {
        m_CurrentState = State::TITLE;
        ShowTitleScreen();
        return;
    }

    const std::size_t worldCount = m_Worlds.size();
    if (Util::Input::IsKeyDown(Util::Keycode::RIGHT) ||
        Util::Input::IsKeyDown(Util::Keycode::D)) {
        m_LevelSelectWorldIndex = (m_LevelSelectWorldIndex + 1) % worldCount;
        ShowWorldSelectScreen();
        if (m_ButtonSFX != nullptr) {
            m_ButtonSFX->Play();
        }
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::LEFT) ||
        Util::Input::IsKeyDown(Util::Keycode::A)) {
        m_LevelSelectWorldIndex =
            (m_LevelSelectWorldIndex + worldCount - 1) % worldCount;
        ShowWorldSelectScreen();
        if (m_ButtonSFX != nullptr) {
            m_ButtonSFX->Play();
        }
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
        Util::Input::IsKeyDown(Util::Keycode::RETURN) ||
        Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        if (m_ButtonSFX != nullptr) {
            m_ButtonSFX->Play();
        }
        ShowLevelSelectScreen();
        m_CurrentState = State::LEVEL_SELECT;
        return;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::TITLE;
        ShowTitleScreen();
        SetTitlePhase(TitlePhase::MENU);
        return;
    }
}

void App::WorldSelect() {
    UpdateWorldSelectScreen();
    m_Root.Update();

    if (Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}
