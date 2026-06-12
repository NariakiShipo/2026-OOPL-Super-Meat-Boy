#include "App.hpp"

#include "common/AssetPath.hpp"

#include "Util/Color.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

#include "game/Collision.hpp"

#include "config.hpp"

namespace {

// ── 版面常數 ────────────────────────────────────────────────
// 圖片原始尺寸 640×480，遊戲座標系以畫面中心為 (0,0)。
// game_x = pixel_x - 320,  game_y = 240 - pixel_y

// 關卡按鈕在圖片中的位置（pixel 座標，以各按鈕中心計）
// 上排  1,2,3,4  (y_px ≈ 220)
// 下排  8,7,6,5  (y_px ≈ 315)
//   col:       1        2        3        4
//   x_px:    105      250      390      540
//
// 轉換後的遊戲座標：
//   上排 game_y = 240 - 220 =  +20
//   下排 game_y = 240 - 315 =  -75

struct LevelButtonLayout {
  glm::vec2 pos;  // 遊戲座標 (game_x, game_y)
  glm::vec2 size; // 點擊碰撞框大小 (pixels, 以 game 空間計)
};

// 按索引 0–7 對應關卡 1–8
// 排列順序：1,2,3,4 上排；8,7,6,5 下排
constexpr float kBtnHalfW = 65.0F; // 按鈕碰撞半寬
constexpr float kBtnHalfH = 28.0F; // 按鈕碰撞半高

// 全域偏移：背景圖被縮放到窗口大小，所以座標直接對應
const std::array<LevelButtonLayout, 8> kForestBtnLayouts = {{
    // 上排：level 1–4
    {{-430.0F, 20.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}}, // 1
    {{-130.0F, 20.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}}, // 2
    {{170.0F, 20.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},  // 3
    {{480.0F, 20.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},  // 4
    // 下排：level 5–8（圖片上是 5 在右、8 在左）
    {{480.0F, -150.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},  // 5
    {{170.0F, -150.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},  // 6
    {{-130.0F, -150.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}}, // 7
    {{-430.0F, -150.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}}, // 8
}};

// Factory 版面與 Forest 相同，用同一份即可
const std::array<LevelButtonLayout, 8> kFactoryBtnLayouts = {{
    {{-460.0F, 60.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{-160.0F, 60.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{140.0F, 60.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{440.0F, 60.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{440.0F, -100.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{140.0F, -100.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{-160.0F, -100.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
    {{-460.0F, -100.0F}, {kBtnHalfW * 4, kBtnHalfH * 2}},
}};

// 分頁按鈕位置（往下移，讓文字不遮標題橫幅）
constexpr float kTabY = 125.0F; // 往下移到畫面底部附近
constexpr float kTabSpacing = 200.0F;
constexpr float kTabZ = 120.0F;
constexpr float kBtnZ = 130.0F;
constexpr float kBgZ = -100.0F;
constexpr float kBackZ = 130.0F;
constexpr float kBackButtonY = -220.0F;

// ── 工具函式 ────────────────────────────────────────────────

bool IsCursorOver(const std::shared_ptr<Util::GameObject> &object) {
  if (object == nullptr || object->GetDrawable() == nullptr) {
    return false;
  }
  const auto cursor = Util::Input::GetCursorPosition();
  const Game::Aabb bounds = Game::GetAabb(object);
  return cursor.x >= bounds.minX && cursor.x <= bounds.maxX &&
         cursor.y >= bounds.minY && cursor.y <= bounds.maxY;
}

// 用隱形 GameObject 作為碰撞框：尺寸由 scale 決定，drawable 不設定
// 所以不會畫出任何東西，但 GetAabb 仍能正常計算
bool IsCursorOverRegion(const glm::vec2 &center, const glm::vec2 &size) {
  const auto cursor = Util::Input::GetCursorPosition();
  return cursor.x >= center.x - size.x * 0.5F &&
         cursor.x <= center.x + size.x * 0.5F &&
         cursor.y >= center.y - size.y * 0.5F &&
         cursor.y <= center.y + size.y * 0.5F;
}

void SetButtonColor(const std::shared_ptr<Util::GameObject> &btn,
                    const Util::Color &color) {
  if (btn == nullptr) {
    return;
  }
  const auto text = std::dynamic_pointer_cast<Util::Text>(btn->GetDrawable());
  if (text != nullptr) {
    text->SetColor(color);
  }
}

std::shared_ptr<Util::GameObject>
MakeTextButton(const std::string &fontPath, const int fontSize,
               const std::string &text, const glm::vec2 &pos,
               const float zIndex,
               const Util::Color &color = Util::Color(255, 255, 255, 255)) {
  auto obj = std::make_shared<Util::GameObject>();
  obj->SetDrawable(
      std::make_shared<Util::Text>(fontPath, fontSize, text, color));
  obj->m_Transform.translation = pos;
  obj->SetZIndex(zIndex);
  return obj;
}

const std::array<LevelButtonLayout, 8> &
GetLayoutForWorld(const std::string &worldName) {
  if (worldName == "Factory") {
    return kFactoryBtnLayouts;
  }
  return kForestBtnLayouts;
}

} // namespace

// ── ShowLevelSelectScreen ─────────────────────────────────────
void App::ShowLevelSelectScreen() {
  // 重置相機：關卡中會縮放相機，切回選關畫面需歸位
  Util::SetCameraZoom(1.0F);
  Util::SetCameraPosition({0.0F, 0.0F});

  m_Root = Util::Renderer();
  m_LevelSelectObjects.clear();
  m_WorldTabButtons.clear();
  m_LevelSelectButtons.clear();

  const std::string fontPath =
      Common::ResolveAssetPath(m_Config.levelSelect.fontPath);

  // ── 背景圖（全屏縮放）────────────────────────────────────
  {
    const std::string &bgPath =
        m_Worlds.empty() ? "" : m_Worlds[m_LevelSelectWorldIndex].bgImagePath;

    m_LevelSelectBackground = std::make_shared<Util::GameObject>();
    if (!bgPath.empty()) {
      const bool isAbsolute =
          bgPath.front() == '/' || bgPath.find(':') != std::string::npos;
      const std::string resolved =
          isAbsolute ? bgPath : Common::ResolveAssetPath(bgPath);
      auto img = std::make_shared<Util::Image>(resolved);
      m_LevelSelectBackground->SetDrawable(img);
      const auto imgSize = img->GetSize();
      m_LevelSelectBackground->m_Transform.scale = {
          static_cast<float>(WINDOW_WIDTH) / imgSize.x,
          static_cast<float>(WINDOW_HEIGHT) / imgSize.y,
      };
    }
    m_LevelSelectBackground->SetZIndex(kBgZ);
    m_LevelSelectObjects.push_back(m_LevelSelectBackground);
  }

  // ── 世界分頁按鈕（文字，位置在畫面底部）──────────────────
  const int numWorlds = static_cast<int>(m_Worlds.size());
  for (int wi = 0; wi < numWorlds; ++wi) {
    const float tabX = -kTabSpacing * 0.5F * (numWorlds - 1) + wi * kTabSpacing;

    const bool isActive =
        (static_cast<std::size_t>(wi) == m_LevelSelectWorldIndex);
    const Util::Color tabColor = isActive ? Util::Color(255, 244, 100, 255)
                                          : Util::Color(200, 200, 200, 255);

    auto tab = MakeTextButton(fontPath, m_Config.levelSelect.tabFontSize,
                              m_Worlds[wi].name, {tabX, kTabY},
                              kTabZ, tabColor);
    m_WorldTabButtons.push_back(tab);
    m_LevelSelectObjects.push_back(tab);
  }

  // ── 隱形點擊區（對齊圖片中的關卡按鈕）─────────────────────
  // 使用沒有 drawable 的 GameObject，靠 GetScaledSize / scale 作碰撞
  // 但因為沒有 drawable 所以 GetAabb 不可靠，改用自訂 IsCursorOverRegion
  // 因此這裡只存位置資訊，不實際創建 GameObject
  // → m_LevelSelectButtons 在此版本存的是「不可見定位用」的 GameObject
  //   （有 scale 讓 GetAabb 有正確大小，但沒有 drawable 所以不渲染）

  m_LevelHoverOverlays.clear();

  const auto &currentWorld =
      m_Worlds.empty() ? Game::WorldData{} : m_Worlds[m_LevelSelectWorldIndex];
  const int numLevels = static_cast<int>(currentWorld.levels.size());
  const auto &layouts = GetLayoutForWorld(currentWorld.name);

  for (int li = 0; li < numLevels && li < static_cast<int>(layouts.size());
       ++li) {
    // 碰撞代理（無 drawable，不渲染）
    auto btn = std::make_shared<Util::GameObject>();
    btn->m_Transform.translation = layouts[li].pos;
    btn->m_Transform.scale = {layouts[li].size.x, layouts[li].size.y};
    btn->SetZIndex(kBtnZ);
    m_LevelSelectButtons.push_back(btn);
  }

  // ── BOSS 按鈕（Forest 第 9 格；只在 Forest 分頁顯示）────────
  m_LevelSelectBossButton = nullptr;
  if (!m_Worlds.empty() &&
      m_Worlds[m_LevelSelectWorldIndex].category ==
          Game::WorldCategory::Forest &&
      m_BossTestLevelIndex < m_Levels.size()) {
    m_LevelSelectBossButton =
        MakeTextButton(fontPath, m_Config.levelSelect.bossFontSize,
                       "BOSS", {480.0F, -240.0F}, kBtnZ,
                       Util::Color(255, 130, 110, 255));
    m_LevelSelectObjects.push_back(m_LevelSelectBossButton);
  }

  // ── Back 按鈕 ────────────────────────────────────────────
  m_LevelSelectBackButton =
      MakeTextButton(fontPath, m_Config.levelSelect.backFontSize,
                     "back", {0.0F, kBackButtonY}, kBackZ,
                     Util::Color(255, 255, 255, 255));
  m_LevelSelectObjects.push_back(m_LevelSelectBackButton);

  // ── 加入 Renderer ────────────────────────────────────────
  for (const auto &obj : m_LevelSelectObjects) {
    m_Root.AddChild(obj);
  }
}

// ── UpdateLevelSelectScreen ───────────────────────────────────
void App::UpdateLevelSelectScreen() {
  // 除錯入口（步驟1）：按 B 直接進 Boss 測試關；之後改為 Forest 第 9 格按鈕
  if (Util::Input::IsKeyDown(Util::Keycode::B) &&
      m_BossTestLevelIndex < m_Levels.size()) {
    LoadLevel(m_BossTestLevelIndex);
    m_CurrentState = State::UPDATE;
    return;
  }

  const Util::Color defaultColor(255, 255, 255, 255);
  const Util::Color hoverColor(255, 244, 100, 255);
  const Util::Color activeTabColor(255, 244, 100, 255);
  const Util::Color inactiveTabColor(200, 200, 200, 255);

  // ── 分頁按鈕 ─────────────────────────────────────────────
  for (std::size_t wi = 0; wi < m_WorldTabButtons.size(); ++wi) {
    const bool isActive = (wi == m_LevelSelectWorldIndex);
    const bool hovered = IsCursorOver(m_WorldTabButtons[wi]);

    SetButtonColor(m_WorldTabButtons[wi],
                   isActive ? activeTabColor
                            : (hovered ? hoverColor : inactiveTabColor));

    if (hovered && !isActive &&
        Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
      m_LevelSelectWorldIndex = wi;
      ShowLevelSelectScreen();
      return;
    }
  }

  // ── 關卡按鈕（點擊圖片上的區域）+ hover overlay ────────
  const auto &currentWorld =
      m_Worlds.empty() ? Game::WorldData{} : m_Worlds[m_LevelSelectWorldIndex];
  const auto &layouts = GetLayoutForWorld(currentWorld.name);

  for (std::size_t li = 0; li < m_LevelSelectButtons.size(); ++li) {
    if (li >= layouts.size()) {
      break;
    }

    const bool hovered = IsCursorOverRegion(layouts[li].pos, layouts[li].size);

    if (hovered && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
      if (li < currentWorld.levels.size()) {
        std::size_t globalOffset = 0;
        for (std::size_t wi2 = 0; wi2 < m_LevelSelectWorldIndex; ++wi2) {
          if (wi2 < m_Worlds.size()) {
            globalOffset += m_Worlds[wi2].levels.size();
          }
        }
        const std::size_t globalIdx = globalOffset + li;
        if (globalIdx < m_Levels.size()) {
          LoadLevel(globalIdx);
          m_CurrentState = State::UPDATE;
        }
      }
      return;
    }
  }

  // ── BOSS 按鈕 ────────────────────────────────────────────
  if (m_LevelSelectBossButton != nullptr) {
    const bool bossHovered = IsCursorOver(m_LevelSelectBossButton);
    SetButtonColor(m_LevelSelectBossButton,
                   bossHovered ? hoverColor : Util::Color(255, 130, 110, 255));
    if (bossHovered && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) &&
        m_BossTestLevelIndex < m_Levels.size()) {
      LoadLevel(m_BossTestLevelIndex);
      m_CurrentState = State::UPDATE;
      return;
    }
  }

  // ── Back 按鈕 ────────────────────────────────────────────
  const bool backHovered = IsCursorOver(m_LevelSelectBackButton);
  SetButtonColor(m_LevelSelectBackButton,
                 backHovered ? hoverColor : defaultColor);

  if (backHovered && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
    m_CurrentState = State::TITLE;
    ShowTitleScreen();
    return;
  }

  if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
    m_CurrentState = State::TITLE;
    ShowTitleScreen();
    return;
  }
}

// ── LevelSelect (每幀呼叫) ────────────────────────────────────
void App::LevelSelect() {
  UpdateLevelSelectScreen();
  m_Root.Update();

  if (Util::Input::IfExit()) {
    m_CurrentState = State::END;
  }
}
