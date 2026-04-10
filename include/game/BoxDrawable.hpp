#ifndef GAME_BOX_DRAWABLE_HPP
#define GAME_BOX_DRAWABLE_HPP

#include "Core/Drawable.hpp"

namespace Game {
class BoxDrawable final : public Core::Drawable {
public:
    explicit BoxDrawable(const glm::vec2 &size)
        : m_Size(size) {}

    void Draw(const Core::Matrices &) override {}

    glm::vec2 GetSize() const override { return m_Size; }

private:
    glm::vec2 m_Size;
};
} // namespace Game

#endif
