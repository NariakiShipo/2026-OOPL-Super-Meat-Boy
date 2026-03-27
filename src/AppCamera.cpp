#include "App.hpp"

#include "Util/TransformUtils.hpp"

void App::UpdateCamera(const float dtMs) {
    const float dtSec = dtMs / 1000.0F;
    const float targetX = m_Player->m_Transform.translation.x;
    const float smooth = std::clamp(6.0F * dtSec, 0.0F, 1.0F);
    m_CameraPosition.x =
        m_CameraPosition.x + (targetX - m_CameraPosition.x) * smooth;

    Util::SetCameraPosition(m_CameraPosition);

    // Keep status text anchored to the screen while world moves.
    if (m_StatusBoard != nullptr) {
        m_StatusBoard->m_Transform.translation =
            m_CameraPosition + glm::vec2{-500.0F, 300.0F};
    }
}
