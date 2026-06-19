#include "App.hpp"

#include <algorithm>
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

// 關卡節點圖：紅梯形節點蛇形排列（Forest 末端附 BOSS 骷髏節點）。
// 方向鍵/AD 沿路徑移動、上下跳列、SPACE/ENTER 進入、ESC 回世界地圖。
// 滑鼠 hover 可選、左鍵進入。

namespace {

constexpr int kNodesPerRow = 5;
constexpr float kNodeStartX = -480.0F;
constexpr float kNodeStepX = 240.0F;
constexpr float kNodeStartY = 110.0F;
constexpr float kNodeStepY = -170.0F;
constexpr glm::vec2 kNodeSize = {120.0F, 84.0F};
constexpr float kNodeZ = 50.0F;

std::shared_ptr<Util::GameObject>
MakeText(const std::string &text, const int fontSize, const glm::vec2 &position,
         const float zIndex, const Util::Color &color) {
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
  auto image =
      std::make_shared<Util::Image>(Common::ResolveAssetPath(relativePath));
  object->SetDrawable(image);
  const auto textureSize = image->GetSize();
  if (textureSize.x > 0.0F && textureSize.y > 0.0F) {
    object->m_Transform.scale = {size.x / textureSize.x,
                                 size.y / textureSize.y};
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

std::string LevelStem(const std::string &mapPath) {
  const std::size_t slashPos = mapPath.find_last_of("/\\");
  const std::size_t nameStart =
      (slashPos == std::string::npos) ? 0 : (slashPos + 1);
  const std::size_t dotPos = mapPath.find_last_of('.');
  if (dotPos == std::string::npos || dotPos < nameStart) {
    return mapPath.substr(nameStart);
  }
  return mapPath.substr(nameStart, dotPos - nameStart);
}

// 蛇形排列：偶數列左→右、奇數列右→左
glm::vec2 SnakePosition(const int index) {
  const int row = index / kNodesPerRow;
  const int col = index % kNodesPerRow;
  const int snakeCol = (row % 2 == 0) ? col : (kNodesPerRow - 1 - col);
  return {kNodeStartX + static_cast<float>(snakeCol) * kNodeStepX,
          kNodeStartY + static_cast<float>(row) * kNodeStepY};
}

bool IsCursorOverRegion(const glm::vec2 &center, const glm::vec2 &size) {
  const auto cursor = Util::Input::GetCursorPosition();
  return cursor.x >= center.x - size.x * 0.5F &&
         cursor.x <= center.x + size.x * 0.5F &&
         cursor.y >= center.y - size.y * 0.5F &&
         cursor.y <= center.y + size.y * 0.5F;
}

} // namespace

void App::ShowLevelSelectScreen() {
  Util::SetCameraZoom(1.0F);
  Util::SetCameraPosition({0.0F, 0.0F});

  m_Root = Util::Renderer();
  m_LevelSelectObjects.clear();
  m_LevelNodes.clear();

  if (m_Worlds.empty()) {
    return;
  }
  m_LevelSelectWorldIndex = m_LevelSelectWorldIndex % m_Worlds.size();
  const auto &world = m_Worlds[m_LevelSelectWorldIndex];

  // 世界全域起始 index（m_Levels 為各世界攤平 + 末端 boss 關）
  std::size_t globalOffset = 0;
  for (std::size_t wi = 0; wi < m_LevelSelectWorldIndex; ++wi) {
    globalOffset += m_Worlds[wi].levels.size();
  }

  // 背景：世界圖滿版淡化 + 綠色頂/底橫幅
  m_LevelSelectBackground = MakeImage(
      world.bgImagePath, {0.0F, 0.0F},
      {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)},
      -100.0F);
  m_LevelSelectObjects.push_back(m_LevelSelectBackground);
  m_LevelSelectObjects.push_back(MakeImage(
      "images/ui_panel.png", {0.0F, 0.0F},
      {static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)},
      -90.0F)); // 半透明壓暗

  const bool hasBoss = (world.category == Game::WorldCategory::Forest) &&
                       (m_BossTestLevelIndex < m_Levels.size());

  // 頂部橫幅
  m_LevelSelectObjects.push_back(MakeImage(
      "images/ui_panel_green.png", {0.0F, 285.0F}, {1100.0F, 100.0F}, 100.0F));
  const std::string chTitle = "CH" +
                              std::to_string(m_LevelSelectWorldIndex + 1) +
                              ": THE " + UpperCase(world.name);
  m_LevelSelectTitle = MakeText(chTitle, 36, {0.0F, 298.0F}, 110.0F,
                                Util::Color(30, 45, 20, 255));
  m_LevelSelectObjects.push_back(m_LevelSelectTitle);
  m_LevelSelectObjects.push_back(MakeText(
      "LEVELS: " + std::to_string(world.levels.size() + (hasBoss ? 1 : 0)), 20,
      {0.0F, 260.0F}, 110.0F, Util::Color(50, 70, 35, 255)));

  // ── 關卡節點 ───────────────────────────────────────────────
  const int levelCount = static_cast<int>(world.levels.size());
  for (int i = 0; i < levelCount; ++i) {
    LevelNode node;
    node.position = SnakePosition(i);
    node.globalLevelIndex = globalOffset + static_cast<std::size_t>(i);
    node.displayName = std::to_string(m_LevelSelectWorldIndex + 1) + "-" +
                       std::to_string(i + 1) + "  " +
                       UpperCase(LevelStem(world.levels[i].mapPath));
    node.tile =
        MakeImage("images/ui_node.png", node.position, kNodeSize, kNodeZ);
    node.label = MakeText(std::to_string(i + 1), 28,
                          {node.position.x, node.position.y + 2.0F},
                          kNodeZ + 1.0F, Util::Color(255, 255, 255, 255));
    m_LevelSelectObjects.push_back(node.tile);
    m_LevelSelectObjects.push_back(node.label);
    m_LevelNodes.push_back(std::move(node));
  }

  if (hasBoss) {
    LevelNode node;
    node.position = SnakePosition(levelCount);
    node.globalLevelIndex = m_BossTestLevelIndex;
    node.displayName = "BOSS  LIL' SLUGGER";
    node.tile =
        MakeImage("images/ui_node_boss.png", node.position, kNodeSize, kNodeZ);
    node.label = nullptr;
    m_LevelSelectObjects.push_back(node.tile);
    m_LevelNodes.push_back(std::move(node));
  }

  // 節點間虛線連接
  for (std::size_t i = 0; i + 1 < m_LevelNodes.size(); ++i) {
    const auto &a = m_LevelNodes[i].position;
    const auto &b = m_LevelNodes[i + 1].position;
    const glm::vec2 delta = b - a;
    const int dashes = 4;
    for (int k = 1; k <= dashes; ++k) {
      const float t = static_cast<float>(k) / (dashes + 1);
      m_LevelSelectObjects.push_back(MakeImage(
          "images/ui_white.png", a + delta * t, {14.0F, 5.0F}, kNodeZ - 1.0F));
    }
  }

  // 底部資訊欄 + 操作提示
  m_LevelSelectObjects.push_back(MakeImage(
      "images/ui_panel_green.png", {0.0F, -300.0F}, {1100.0F, 95.0F}, 100.0F));
  m_LevelSelBottomText = MakeText("SELECT A LEVEL", 30, {0.0F, -290.0F}, 110.0F,
                                  Util::Color(30, 45, 20, 255));
  m_LevelSelectObjects.push_back(m_LevelSelBottomText);
  m_LevelSelectObjects.push_back(MakeText("[SPACE] SELECT", 20,
                                          {-460.0F, -325.0F}, 110.0F,
                                          Util::Color(45, 60, 30, 255)));
  m_LevelSelectObjects.push_back(MakeText("[ESC] BACK", 20, {460.0F, -325.0F},
                                          110.0F,
                                          Util::Color(45, 60, 30, 255)));

  for (const auto &object : m_LevelSelectObjects) {
    m_Root.AddChild(object);
  }

  m_LevelNodeIndex = 0;
}

void App::UpdateLevelSelectScreen() {
  if (m_LevelNodes.empty()) {
    if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
      ShowWorldSelectScreen();
      m_CurrentState = State::WORLD_SELECT;
    }
    return;
  }

  const int nodeCount = static_cast<int>(m_LevelNodes.size());

  // 除錯捷徑：B 直接進 Boss 關
  if (Util::Input::IsKeyDown(Util::Keycode::B) &&
      m_BossTestLevelIndex < m_Levels.size()) {
    LoadLevel(m_BossTestLevelIndex);
    m_CurrentState = State::UPDATE;
    return;
  }

  // ── 鍵盤導航（沿蛇形路徑）─────────────────────────────────
  bool confirm = false;
  if (Util::Input::IsKeyDown(Util::Keycode::RIGHT) ||
      Util::Input::IsKeyDown(Util::Keycode::D)) {
    m_LevelNodeIndex = std::min(m_LevelNodeIndex + 1, nodeCount - 1);
  }
  if (Util::Input::IsKeyDown(Util::Keycode::LEFT) ||
      Util::Input::IsKeyDown(Util::Keycode::A)) {
    m_LevelNodeIndex = std::max(m_LevelNodeIndex - 1, 0);
  }
  if (Util::Input::IsKeyDown(Util::Keycode::DOWN) ||
      Util::Input::IsKeyDown(Util::Keycode::S)) {
    m_LevelNodeIndex = std::min(m_LevelNodeIndex + kNodesPerRow, nodeCount - 1);
  }
  if (Util::Input::IsKeyDown(Util::Keycode::UP) ||
      Util::Input::IsKeyDown(Util::Keycode::W)) {
    m_LevelNodeIndex = std::max(m_LevelNodeIndex - kNodesPerRow, 0);
  }
  if (Util::Input::IsKeyDown(Util::Keycode::SPACE) ||
      Util::Input::IsKeyDown(Util::Keycode::RETURN)) {
    confirm = true;
  }

  // ── 滑鼠 hover / 點擊 ─────────────────────────────────────
  for (int i = 0; i < nodeCount; ++i) {
    if (IsCursorOverRegion(m_LevelNodes[i].position, kNodeSize)) {
      m_LevelNodeIndex = i;
      if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        confirm = true;
      }
    }
  }

  // ── 視覺：選取節點放大、底欄顯示名稱 ──────────────────────
  for (int i = 0; i < nodeCount; ++i) {
    auto &node = m_LevelNodes[i];
    const bool selected = (i == m_LevelNodeIndex);
    const float baseScaleX = kNodeSize.x / 120.0F; // ui_node 原始 120×84
    const float baseScaleY = kNodeSize.y / 84.0F;
    const float factor = selected ? 1.18F : 1.0F;
    if (node.tile != nullptr) {
      node.tile->m_Transform.scale = {baseScaleX * factor, baseScaleY * factor};
    }
    if (node.label != nullptr) {
      const auto labelText =
          std::dynamic_pointer_cast<Util::Text>(node.label->GetDrawable());
      if (labelText != nullptr) {
        labelText->SetColor(selected ? Util::Color(255, 244, 100, 255)
                                     : Util::Color(255, 255, 255, 255));
      }
    }
  }
  if (m_LevelSelBottomText != nullptr) {
    const auto bottomText = std::dynamic_pointer_cast<Util::Text>(
        m_LevelSelBottomText->GetDrawable());
    if (bottomText != nullptr) {
      bottomText->SetText(m_LevelNodes[m_LevelNodeIndex].displayName);
    }
  }

  if (confirm) {
    const std::size_t globalIdx =
        m_LevelNodes[m_LevelNodeIndex].globalLevelIndex;
    if (globalIdx < m_Levels.size()) {
      if (m_ButtonSFX != nullptr) {
        m_ButtonSFX->Play();
      }
      LoadLevel(globalIdx);
      m_CurrentState = State::UPDATE;
    }
    return;
  }

  if (Util::Input::IsKeyDown(Util::Keycode::ESCAPE)) {
    ShowWorldSelectScreen();
    m_CurrentState = State::WORLD_SELECT;
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
