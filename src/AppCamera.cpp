#include "App.hpp"

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
            m_CameraPosition + glm::vec2{-500.0F, 300.0F};
    }
}
