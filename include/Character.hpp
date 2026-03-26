#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include <cmath>
#include <string>

#include "Util/GameObject.hpp"

class Character : public Util::GameObject {
public:
    explicit Character(const std::string& ImagePath);

    Character(const Character&) = delete;

    Character(Character&&) = delete;

    Character& operator=(const Character&) = delete;

    Character& operator=(Character&&) = delete;

    [[nodiscard]] const std::string& GetImagePath() const { return m_ImagePath; }

    [[nodiscard]] const glm::vec2& GetPosition() const { return m_Transform.translation; }

    [[nodiscard]] bool GetVisibility() const { return m_Visible; }

    void SetImage(const std::string& ImagePath);

    void SetPosition(const glm::vec2& Position) { m_Transform.translation = Position; }

    
    // TODO: Implement the collision detection
    [[nodiscard]] bool IfCollides(const std::shared_ptr<Character>& other) const {
        if (!other) {
            return false;
        }

        // AABB overlap check with position treated as box center.
        const auto thisPos = GetPosition();
        const auto otherPos = other->GetPosition();

        const auto thisHalfSize = GetScaledSize() * 0.5F;
        const auto otherHalfSize = other->GetScaledSize() * 0.5F;

        const bool overlapX = std::abs(thisPos.x - otherPos.x) <= (thisHalfSize.x + otherHalfSize.x);
        const bool overlapY = std::abs(thisPos.y - otherPos.y) <= (thisHalfSize.y + otherHalfSize.y);

        return overlapX && overlapY;
    }

    // TODO: Add and implement more methods and properties as needed to finish Giraffe Adventure.

private:
    void ResetPosition() { m_Transform.translation = {0, 0}; }

    std::string m_ImagePath;
};


#endif //CHARACTER_HPP
