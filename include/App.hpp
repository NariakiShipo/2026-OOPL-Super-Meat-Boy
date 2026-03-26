#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include <memory>
#include <vector>

#include "Util/GameObject.hpp"
#include "Util/Renderer.hpp"
#include "TileMap.hpp"

class App {
public:
    enum class State {
        START,
        UPDATE,
        END,
    };

    State GetCurrentState() const { return m_CurrentState; }

    void Start();

    void Update();

    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    bool LoadCollisionMap(const std::string &mapPath);
    bool LoadVisualMap(const std::string &mapPath);
    bool IsSolidTile(int col, int row) const;
    bool CollidesAt(const glm::vec2 &position) const;
    void UpdatePlayerPhysics(float dtSeconds);

private:
    State m_CurrentState = State::START;

    Util::Renderer m_Renderer;
    std::shared_ptr<Util::GameObject> m_Player;
    std::vector<std::shared_ptr<Util::GameObject>> m_TileObjects;

    glm::vec2 m_PlayerVelocity = {0.0F, 0.0F};
    bool m_IsGrounded = false;

    float m_MoveSpeed = 240.0F;
    float m_Gravity = -1200.0F;
    float m_JumpVelocity = 520.0F;
    float m_MaxFallSpeed = 900.0F;

    std::size_t m_MapWidth = 0;
    std::size_t m_MapHeight = 0;
    float m_TileWidth = 0.0F;
    float m_TileHeight = 0.0F;
    float m_MapPixelWidth = 0.0F;
    float m_MapPixelHeight = 0.0F;
    std::vector<bool> m_SolidTiles;
};

#endif
