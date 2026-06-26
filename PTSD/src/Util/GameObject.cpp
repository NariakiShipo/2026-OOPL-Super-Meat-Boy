#include "Util/GameObject.hpp"
#include "Util/Transform.hpp"
#include "Util/TransformUtils.hpp"

namespace Util {

void GameObject::Draw() {
    if (!m_Visible || m_Drawable == nullptr) {
        return;
    }

    const auto size = m_Drawable->GetSize();
    if (size.x <= 0.0F || size.y <= 0.0F) {
        return;
    }

    auto data = Util::ConvertToUniformBufferData(m_Transform, size, m_ZIndex);
    data.m_Model = glm::translate(
        data.m_Model, glm::vec3{m_Pivot / size, 0} * -1.0F);

    m_Drawable->Draw(data);
}

} // namespace Util
