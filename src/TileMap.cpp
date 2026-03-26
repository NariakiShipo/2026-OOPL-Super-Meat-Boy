#include "TileMap.hpp"

#include "Core/IndexBuffer.hpp"
#include "Core/TextureUtils.hpp"
#include "Core/VertexBuffer.hpp"
#include "Util/Logger.hpp"
#include "config.hpp"

std::unique_ptr<Core::Program> TileMap::s_Program;
std::unique_ptr<Core::VertexArray> TileMap::s_VertexArray;

TileMap::TileMap(SDL_Surface *surface) {
    if (s_Program == nullptr) {
        InitProgram();
    }
    if (s_VertexArray == nullptr) {
        InitVertexArray();
    }

    m_UniformBuffer = std::make_unique<Core::UniformBuffer<Core::Matrices>>(
        *s_Program, "Matrices", 0);

    m_Size = {static_cast<float>(surface->w), static_cast<float>(surface->h)};

    m_Texture = std::make_unique<Core::Texture>(
        Core::SdlFormatToGlFormat(surface->format->format),
        surface->w, surface->h, surface->pixels);

    SDL_FreeSurface(surface);
}

void TileMap::Draw(const Core::Matrices &data) {
    m_UniformBuffer->SetData(0, data);

    m_Texture->Bind(UNIFORM_SURFACE_LOCATION);
    s_Program->Bind();
    s_Program->Validate();

    s_VertexArray->Bind();
    s_VertexArray->DrawTriangles();
}

void TileMap::InitProgram() {
    s_Program = std::make_unique<Core::Program>(
        RESOURCE_DIR "/shaders/Tile.vert",
        RESOURCE_DIR "/shaders/Tile.frag");
    s_Program->Bind();

    const GLint loc = glGetUniformLocation(s_Program->GetId(), "surface");
    glUniform1i(loc, UNIFORM_SURFACE_LOCATION);
}

void TileMap::InitVertexArray() {
    s_VertexArray = std::make_unique<Core::VertexArray>();

    // Vertex positions (unit square, centred at origin)
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            -0.5F,  0.5F,  // top-left
            -0.5F, -0.5F,  // bottom-left
             0.5F, -0.5F,  // bottom-right
             0.5F,  0.5F,  // top-right
        },
        2));

    // UV coords (matching Util::Image convention: (0,0) at top-left vertex)
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            0.0F, 0.0F,  // top-left
            0.0F, 1.0F,  // bottom-left
            1.0F, 1.0F,  // bottom-right
            1.0F, 0.0F,  // top-right
        },
        2));

    s_VertexArray->SetIndexBuffer(
        std::make_unique<Core::IndexBuffer>(std::vector<unsigned int>{
            0, 1, 2,
            0, 2, 3,
        }));
}
