#include "App.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "common/AssetPath.hpp"
#include "game/Collision.hpp"

#include "Util/Animation.hpp"
#include "Util/Image.hpp"
#include "Util/Logger.hpp"

// 旋轉鋸：鋸片繞軸心等速旋轉。
//   dual=true  → 雙鋸對轉：旋臂貫穿軸心（長 2R），兩端各一鋸
//   dual=false → 單鋸繞軸：旋臂自軸心伸出（長 R），末端一鋸
// 資料來源：TMX object layer "rotors"（橢圓：中心=軸心、寬/2=旋臂半徑、
// 高=鋸片直徑；物件名 "single" = 單鋸）
// 視覺：旋臂 GameObject 依角度旋轉 + buzzsaw 動畫鋸片；判定：鋸片中心縮小 AABB。
// 各組以軸心 x 推導固定相位差，避免全場同步擺動。

namespace {
std::vector<std::string> ResolveFramePaths(const std::vector<std::string> &framePaths) {
    std::vector<std::string> resolvedPaths;
    resolvedPaths.reserve(framePaths.size());
    for (const auto &framePath : framePaths) {
        const bool hasAbsolutePath =
            !framePath.empty() &&
            (framePath.front() == '/' || framePath.find(':') != std::string::npos);
        resolvedPaths.push_back(
            hasAbsolutePath ? framePath : Common::ResolveAssetPath(framePath));
    }
    return resolvedPaths;
}

glm::vec2 SawCenter(const Game::RotorConfig &cfg, const float angleDeg,
                    const float side) {
    const float rad = glm::radians(angleDeg);
    return cfg.pivot + side * cfg.armRadius * glm::vec2{std::cos(rad), std::sin(rad)};
}
} // namespace

void App::SpawnRotors(const std::vector<Game::RotorConfig> &configs) {
    m_Rotors.clear();
    if (configs.empty()) {
        return;
    }

    const auto sawFramePaths = ResolveFramePaths(m_Config.buzzsaw.animFrames);
    const std::size_t sawInterval =
        std::max<std::size_t>(m_Config.buzzsaw.animIntervalMs, 1);

    for (const auto &cfg : configs) {
        RotorState rotor;
        rotor.config = cfg;
        // 起始角由關卡資料指定（TMX 物件 rotation），各組可自行錯開
        rotor.angleDeg = std::fmod(cfg.startAngleDeg, 360.0F);
        if (rotor.angleDeg < 0.0F) {
            rotor.angleDeg += 360.0F;
        }

        // 旋臂：雙鋸臂長 2R（中心=軸心）、單鋸臂長 R（中心=軸心外 R/2）
        rotor.bar = std::make_shared<Util::GameObject>();
        auto barImage = std::make_shared<Util::Image>(
            Common::ResolveAssetPath(m_Config.rotor.barTexturePath));
        rotor.bar->SetDrawable(barImage);
        const auto barTexSize = barImage->GetSize();
        if (barTexSize.x > 0.0F && barTexSize.y > 0.0F) {
            const float barLength =
                cfg.dual ? (cfg.armRadius * 2.0F) : cfg.armRadius;
            rotor.bar->m_Transform.scale = {
                barLength / barTexSize.x,
                m_Config.rotor.barThickness / barTexSize.y,
            };
        }
        rotor.bar->m_Transform.translation = cfg.pivot;
        rotor.bar->SetZIndex(m_Config.rotor.barZIndex);
        rotor.bar->SetVisible(true);

        // 兩端鋸片
        const auto makeSaw = [&]() {
            auto saw = std::make_shared<Util::GameObject>();
            if (!sawFramePaths.empty()) {
                auto anim = std::make_shared<Util::Animation>(
                    sawFramePaths, true, sawInterval, true, 0);
                saw->SetDrawable(anim);
                const auto frameSize = anim->GetSize();
                if (frameSize.x > 0.0F && frameSize.y > 0.0F) {
                    saw->m_Transform.scale = {
                        cfg.sawDiameter / frameSize.x,
                        cfg.sawDiameter / frameSize.y,
                    };
                }
            }
            saw->SetZIndex(m_Config.rotor.sawZIndex);
            saw->SetVisible(true);
            return saw;
        };
        rotor.sawA = makeSaw();
        rotor.sawA->m_Transform.translation = SawCenter(cfg, rotor.angleDeg, 1.0F);
        if (cfg.dual) {
            rotor.sawB = makeSaw();
            rotor.sawB->m_Transform.translation =
                SawCenter(cfg, rotor.angleDeg, -1.0F);
        }

        m_Rotors.push_back(std::move(rotor));
    }

    LOG_INFO("Spawned {} rotor(s)", m_Rotors.size());
}

void App::UpdateRotors(const float dtMs) {
    if (m_Rotors.empty()) {
        return;
    }

    const float deltaDeg = m_Config.rotor.angularSpeedDegPerSec * dtMs / 1000.0F;
    for (auto &rotor : m_Rotors) {
        rotor.angleDeg = std::fmod(rotor.angleDeg + deltaDeg, 360.0F);

        const float rad = glm::radians(rotor.angleDeg);
        const glm::vec2 dir = {std::cos(rad), std::sin(rad)};

        if (rotor.bar != nullptr) {
            // 單鋸臂繞「端點」旋轉：把臂中心放在軸心外 R/2 處
            rotor.bar->m_Transform.translation =
                rotor.config.dual
                    ? rotor.config.pivot
                    : rotor.config.pivot + dir * (rotor.config.armRadius * 0.5F);
            rotor.bar->m_Transform.rotation = rad;
        }
        if (rotor.sawA != nullptr) {
            rotor.sawA->m_Transform.translation =
                rotor.config.pivot + dir * rotor.config.armRadius;
        }
        if (rotor.sawB != nullptr) {
            rotor.sawB->m_Transform.translation =
                rotor.config.pivot - dir * rotor.config.armRadius;
        }
    }
}

void App::CheckRotorPlayerCollisions() {
    if (m_Rotors.empty() || m_Player == nullptr) {
        return;
    }

    const auto playerAabb =
        Game::MakeAabb(m_Player->m_Transform.translation, m_PlayerColliderSize);

    for (const auto &rotor : m_Rotors) {
        const float hitSize = rotor.config.sawDiameter * m_Config.rotor.hitboxScale;
        const glm::vec2 hitBox = {hitSize, hitSize};

        for (const float side : {1.0F, -1.0F}) {
            if (side < 0.0F && !rotor.config.dual) {
                continue;  // 單鋸沒有另一端
            }
            const auto center = SawCenter(rotor.config, rotor.angleDeg, side);
            if (Game::IsOverlap(playerAabb, Game::MakeAabb(center, hitBox))) {
                LOG_INFO("Player hit rotor saw at ({:.1f}, {:.1f})",
                         center.x, center.y);
                RespawnPlayer();  // 內含 ResetBoss()
                return;
            }
        }
    }
}
