#include "game/Collision.hpp"

namespace Game {
Aabb MakeAabb(const glm::vec2 &center, const glm::vec2 &size) {
    const auto half = size * 0.5F;
    return {
        center.x - half.x,
        center.x + half.x,
        center.y - half.y,
        center.y + half.y,
    };
}

Aabb GetAabb(const std::shared_ptr<Util::GameObject> &obj) {
    return MakeAabb(obj->m_Transform.translation, obj->GetScaledSize());
}

bool IsOverlap(const Aabb &a, const Aabb &b) {
    return a.minX < b.maxX && a.maxX > b.minX && a.minY < b.maxY &&
           a.maxY > b.minY;
}
} // namespace Game
