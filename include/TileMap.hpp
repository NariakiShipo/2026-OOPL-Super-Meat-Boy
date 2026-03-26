#ifndef TILE_MAP_HPP
#define TILE_MAP_HPP

#include "pch.hpp"

#include "Core/Drawable.hpp"
#include "Core/Program.hpp"
#include "Core/Texture.hpp"
#include "Core/UniformBuffer.hpp"
#include "Core/VertexArray.hpp"

/**
 * @brief A Core::Drawable that renders a pre-baked tile layer as a single textured quad.
 *
 * The caller composites all tiles for one map layer onto a single SDL_Surface
 * and passes ownership here. TileMap uploads it to the GPU and renders it as
 * a textured quad whose GetSize() matches the surface dimensions.
 */
class TileMap : public Core::Drawable {
public:
    /**
     * @brief Construct from a pre-baked SDL_Surface.
     * @param surface Owning pointer. Freed after GPU upload.
     */
    explicit TileMap(SDL_Surface *surface);

    glm::vec2 GetSize() const override { return m_Size; }

    void Draw(const Core::Matrices &data) override;

private:
    static void InitProgram();
    static void InitVertexArray();

    static constexpr int UNIFORM_SURFACE_LOCATION = 0;

    static std::unique_ptr<Core::Program> s_Program;
    static std::unique_ptr<Core::VertexArray> s_VertexArray;

    std::unique_ptr<Core::UniformBuffer<Core::Matrices>> m_UniformBuffer;
    std::unique_ptr<Core::Texture> m_Texture;
    glm::vec2 m_Size;
};

#endif
