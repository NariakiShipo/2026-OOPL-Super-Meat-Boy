#include "App.hpp"

#include <algorithm>
#include <fstream>

#include "common/AssetPath.hpp"

#include "Util/Image.hpp"
#include "Util/Logger.hpp"

// 建立角色清單（JSON 缺漏時用程式內預設後備），並夾住目前所選 index。
void App::InitCharacters() {
    m_Characters = m_Config.characters;
    if (m_Characters.empty()) {
        CharacterDef meatBoy{};
        meatBoy.name = "MEAT BOY";
        meatBoy.description = "THE FEARLESS RED HERO.";
        meatBoy.idleSpritePath = m_Config.player.idleSpritePath;
        meatBoy.runLeftSpritePath = m_Config.player.runLeftSpritePath;
        meatBoy.runRightSpritePath = m_Config.player.runRightSpritePath;
        meatBoy.unlockCost = 0;
        m_Characters.push_back(meatBoy);
    }
    m_SelectedCharacter = std::clamp(
        m_SelectedCharacter, 0, static_cast<int>(m_Characters.size()) - 1);
    m_CharacterBrowseIndex = m_SelectedCharacter;

    // 以「預設角色 idle 在 player.scale 下的世界高度」為基準高度，
    // 之後每個角色都縮放到此高度，避免不同來源像素尺寸（如 20x20 vs 300x300）
    // 造成換角色後主角忽大忽小、碰撞框錯亂。
    m_PlayerTargetHeight = 0.0F;
    if (!m_Characters.empty()) {
        auto referenceIdle = std::make_shared<Util::Image>(
            Common::ResolveAssetPath(m_Characters.front().idleSpritePath));
        m_PlayerTargetHeight =
            referenceIdle->GetSize().y * m_Config.player.scale;
    }
}

// 累積繃帶數 >= 解鎖門檻 → 已解鎖。
bool App::IsCharacterUnlocked(int index) const {
    if (index < 0 || index >= static_cast<int>(m_Characters.size())) {
        return false;
    }
    return m_BandagesCollected >= m_Characters[index].unlockCost;
}

// 把目前所選角色的貼圖套到主角（重建 idle/run 並重算碰撞框）。
void App::ApplyCharacterToPlayer() {
    if (m_Characters.empty() || m_Player == nullptr) {
        return;
    }
    m_SelectedCharacter = std::clamp(
        m_SelectedCharacter, 0, static_cast<int>(m_Characters.size()) - 1);
    const auto &ch = m_Characters[m_SelectedCharacter];

    m_PlayerIdleDrawable =
        std::make_shared<Util::Image>(Common::ResolveAssetPath(ch.idleSpritePath));
    m_PlayerRunLeftDrawable = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(ch.runLeftSpritePath));
    m_PlayerRunRightDrawable = std::make_shared<Util::Image>(
        Common::ResolveAssetPath(ch.runRightSpritePath));
    m_PlayerJumpDrawable = m_PlayerIdleDrawable;
    m_PlayerFallDrawable = m_PlayerIdleDrawable;

    // 依角色 idle 圖的原生尺寸正規化縮放，讓每個角色都呈現相同世界高度。
    const auto idleSize = m_PlayerIdleDrawable->GetSize();
    float uniformScale = m_Config.player.scale;
    if (idleSize.y > 0.0F && m_PlayerTargetHeight > 0.0F) {
        uniformScale = m_PlayerTargetHeight / idleSize.y;
    }
    m_Player->m_Transform.scale = {uniformScale, uniformScale};

    // 立刻換到 idle，並把狀態重置；下一幀動畫狀態機會自動套到正確方向的圖。
    m_Player->SetDrawable(m_PlayerIdleDrawable);
    m_PlayerAnimState = PlayerAnimState::IDLE;
    m_PlayerRunDrawableFacingRight = m_PlayerFacingRight;
    m_PlayerColliderSize = idleSize * uniformScale;

    LOG_INFO("Character applied: {} (index {})", ch.name, m_SelectedCharacter);
}

// 讀取進度（繃帶總數 + 所選角色）。檔案不存在則維持預設。
void App::LoadProgress() {
    std::ifstream input(m_ProgressPath);
    int bandages = m_BandagesCollected;
    int selected = m_SelectedCharacter;
    if (input >> bandages >> selected) {
        m_BandagesCollected = std::max(0, bandages);
        m_SelectedCharacter = selected;
    }
    if (!m_Characters.empty()) {
        m_SelectedCharacter = std::clamp(
            m_SelectedCharacter, 0, static_cast<int>(m_Characters.size()) - 1);
    }
    // 所選角色若未達解鎖門檻（門檻被調高 / 存檔不一致）→ 退回預設角色。
    if (!IsCharacterUnlocked(m_SelectedCharacter)) {
        m_SelectedCharacter = 0;
    }
    m_CharacterBrowseIndex = m_SelectedCharacter;
}

void App::SaveProgress() const {
    std::ofstream output(m_ProgressPath, std::ios::trunc);
    if (output) {
        output << m_BandagesCollected << ' ' << m_SelectedCharacter << '\n';
    }
}
