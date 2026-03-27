#ifndef GAME_COLLISION_HPP
#define GAME_COLLISION_HPP

#include "Util/GameObject.hpp"

namespace Game {
struct Aabb {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

Aabb MakeAabb(const glm::vec2 &center, const glm::vec2 &size);
Aabb GetAabb(const std::shared_ptr<Util::GameObject> &obj);
bool IsOverlap(const Aabb &a, const Aabb &b);
} // namespace Game

#endif
