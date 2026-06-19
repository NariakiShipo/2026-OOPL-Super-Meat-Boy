#include "App.hpp"

#include <algorithm>
#include <string>

#include "Util/Logger.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

void App::UpdateCamera(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    const auto playerPosition = m_Player->m_Transform.translation;

    glm::vec2 rawLookahead = {0.0F, 0.0F};
    rawLookahead.x =
        std::clamp(m_PlayerVelocity.x * m_CameraLookaheadTime,
                   -m_CameraLookaheadMaxX, m_CameraLookaheadMaxX);
    if (m_PlayerVelocity.y > 0.0F) {
        rawLookahead.y = std::clamp(m_PlayerVelocity.y * m_CameraLookaheadTime,
                                    0.0F, m_CameraLookaheadMaxUp);
    } else {
        rawLookahead.y =
            std::clamp(m_PlayerVelocity.y * m_CameraLookaheadTime,
                       -m_CameraLookaheadMaxDown, 0.0F);
    }

    const float lookaheadLerp =
        std::clamp(m_CameraLookaheadResponse * dtSec, 0.0F, 1.0F);
    m_CameraLookaheadOffset +=
        (rawLookahead - m_CameraLookaheadOffset) * lookaheadLerp;

    const glm::vec2 desiredFocus = playerPosition + m_CameraLookaheadOffset;

    glm::vec2 target = m_CameraPosition;
    const float left = m_CameraPosition.x - m_CameraDeadZoneHalfWidth;
    const float right = m_CameraPosition.x + m_CameraDeadZoneHalfWidth;
    const float bottom = m_CameraPosition.y - m_CameraDeadZoneHalfHeight;
    const float top = m_CameraPosition.y + m_CameraDeadZoneHalfHeight;

    if (desiredFocus.x < left) {
        target.x = desiredFocus.x + m_CameraDeadZoneHalfWidth;
    } else if (desiredFocus.x > right) {
        target.x = desiredFocus.x - m_CameraDeadZoneHalfWidth;
    }

    if (desiredFocus.y < bottom) {
        target.y = desiredFocus.y + m_CameraDeadZoneHalfHeight;
    } else if (desiredFocus.y > top) {
        target.y = desiredFocus.y - m_CameraDeadZoneHalfHeight;
    }

    target.x = std::clamp(target.x, m_CameraBoundsMin.x, m_CameraBoundsMax.x);
    target.y = std::clamp(target.y, m_CameraBoundsMin.y, m_CameraBoundsMax.y);

    const float smooth = std::clamp(m_CameraFollowSpeed * dtSec, 0.0F, 1.0F);
    m_CameraPosition += (target - m_CameraPosition) * smooth;

    m_CameraPosition.x =
        std::clamp(m_CameraPosition.x, m_CameraBoundsMin.x, m_CameraBoundsMax.x);
    m_CameraPosition.y =
        std::clamp(m_CameraPosition.y, m_CameraBoundsMin.y, m_CameraBoundsMax.y);

    Util::SetCameraPosition(m_CameraPosition);

    // Keep status text anchored to the screen while world moves.
    if (m_StatusBoard != nullptr) {
        m_StatusBoard->m_Transform.translation =
            m_CameraPosition + m_Config.ui.statusOffset + m_Config.ui.statusText.position;
    }

    // 作弊提示字固定在螢幕下方中央，並依作弊狀態顯示/隱藏。
    if (m_CheatIndicator != nullptr) {
        m_CheatIndicator->m_Transform.translation =
            m_CameraPosition + glm::vec2{0.0F, -330.0F};
        m_CheatIndicator->SetVisible(m_CheatSawImmune);
    }

    // HUD（計時器 / 繃帶數量）同樣鎖定螢幕，跟著相機移動。
    if (m_TimerText != nullptr) {
        m_TimerText->m_Transform.translation =
            m_CameraPosition + m_Config.ui.timerText.position;
    }
    if (m_BandageCountText != nullptr) {
        m_BandageCountText->m_Transform.translation =
            m_CameraPosition + m_Config.ui.bandageText.position;
    }
}

// A+ 門檻：依地圖大小（寬+高）自動換算秒數，並夾住下限。
float App::ComputeGradeThresholdSec(const glm::vec2 &mapPixelSize) const {
    const float pps = std::max(1.0F, m_Config.grading.pixelsPerSecond);
    const float raw = (mapPixelSize.x + mapPixelSize.y) / pps;
    return std::max(m_Config.grading.minSeconds, raw);
}

// 過關當下：判定成績、顯示通關 UI、設定通關鏡頭目標。
void App::TriggerLevelComplete() {
    const float timeSec = m_LevelTimeMs / 1000.0F;
    const bool aPlus = (timeSec <= m_GradeThresholdSec);

    if (m_GradeText != nullptr) {
        const auto text =
            std::dynamic_pointer_cast<Util::Text>(m_GradeText->GetDrawable());
        if (text != nullptr) {
            text->SetText(aPlus ? "GRADE A+" : "GRADE A");
            text->SetColor(aPlus ? Util::Color(220, 40, 40, 255)
                                 : Util::Color(235, 170, 40, 255));
        }
    }
    LOG_INFO("Level complete: time={:.2f}s threshold={:.2f}s grade={}", timeSec,
             m_GradeThresholdSec, aPlus ? "A+" : "A");

    // 通關鏡頭：聚焦主角與終點（女孩）中點並放大
    glm::vec2 focus = (m_Player != nullptr) ? m_Player->m_Transform.translation
                                            : m_CameraPosition;
    if (m_GoalFlag != nullptr) {
        focus = (focus + m_GoalFlag->m_Transform.translation) * 0.5F;
    }
    m_VictoryFocus = focus;
    m_VictoryZoomTarget =
        m_CameraZoom * std::max(1.0F, m_Config.grading.victoryZoomFactor);

    // 顯示通關 UI（立即依目前相機定位，避免第一幀位置偏移）、隱藏遊戲 HUD / 狀態列
    for (std::size_t i = 0; i < m_LevelCompleteObjects.size(); ++i) {
        if (m_LevelCompleteObjects[i] == nullptr) {
            continue;
        }
        if (i < m_LevelCompleteBaseOffsets.size()) {
            m_LevelCompleteObjects[i]->m_Transform.translation =
                m_CameraPosition + m_LevelCompleteBaseOffsets[i];
        }
        m_LevelCompleteObjects[i]->SetVisible(true);
    }
    if (m_StatusBoard != nullptr) {
        m_StatusBoard->SetVisible(false);
    }
    if (m_TimerText != nullptr) {
        m_TimerText->SetVisible(false);
    }
    if (m_BandageCountText != nullptr) {
        m_BandageCountText->SetVisible(false);
    }
}

// 過關後每幀：鏡頭平滑放大並聚焦，通關 UI 跟著相機鎖定螢幕。
void App::UpdateVictoryCamera(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    const float lerp =
        std::clamp(m_Config.grading.victoryCameraLerp * dtSec, 0.0F, 1.0F);

    m_CameraPosition += (m_VictoryFocus - m_CameraPosition) * lerp;
    m_CameraZoom += (m_VictoryZoomTarget - m_CameraZoom) * lerp;
    Util::SetCameraPosition(m_CameraPosition);
    Util::SetCameraZoom(m_CameraZoom);

    for (std::size_t i = 0; i < m_LevelCompleteObjects.size() &&
                            i < m_LevelCompleteBaseOffsets.size();
         ++i) {
        if (m_LevelCompleteObjects[i] != nullptr) {
            m_LevelCompleteObjects[i]->m_Transform.translation =
                m_CameraPosition + m_LevelCompleteBaseOffsets[i];
        }
    }
}

// 重生 / 載入新關：隱藏通關 UI、還原 HUD 與狀態列。
void App::HideLevelCompleteUI() {
    for (const auto &object : m_LevelCompleteObjects) {
        if (object != nullptr) {
            object->SetVisible(false);
        }
    }
    if (m_StatusBoard != nullptr) {
        m_StatusBoard->SetVisible(true);
    }
    if (m_TimerText != nullptr) {
        m_TimerText->SetVisible(true);
    }
    if (m_BandageCountText != nullptr) {
        m_BandageCountText->SetVisible(true);
    }
}
