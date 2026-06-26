// FIXME: this file should be refactor, API change reference from Image.cpp

#include "Core/Texture.hpp"
#include "Core/TextureUtils.hpp"

#include "Util/Logger.hpp"
#include "Util/Text.hpp"
#include "Util/TransformUtils.hpp"

#include "config.hpp"

namespace Util {
Text::Text(const std::string &font, int fontSize, const std::string &text,
           const Util::Color &color)
    : m_Text(text),
      m_Color(color) {
    if (s_Program == nullptr) {
        InitProgram();
    }
    if (s_VertexArray == nullptr) {
        InitVertexArray();
    }

    m_UniformBuffer = std::make_unique<Core::UniformBuffer<Core::Matrices>>(
        *s_Program, "Matrices", 0);

    m_Font = {TTF_OpenFont(font.c_str(), fontSize), TTF_CloseFont};

    auto raw_surface = TTF_RenderUTF8_Blended_Wrapped(m_Font.get(),
                                                     m_Text.c_str(),
                                                     m_Color.ToSdlColor(),
                                                     0);

    std::unique_ptr<SDL_Surface, std::function<void(SDL_Surface *)>> surface(
        raw_surface, SDL_FreeSurface);

    if (surface == nullptr) {
        LOG_ERROR("Failed to create text: {}", TTF_GetError());
        // Fallback: create a 1x1 white surface so rendering can continue.
        SDL_Surface *fallback = SDL_CreateRGBSurfaceWithFormat(
            0, 1, 1, 32, SDL_PIXELFORMAT_ABGR8888);
        if (fallback != nullptr) {
            Uint32 color = SDL_MapRGBA(fallback->format, 255, 255, 255, 255);
            SDL_FillRect(fallback, nullptr, color);
            surface.reset(fallback);
        }
    }

    if (surface == nullptr) {
        // As a last resort, create a minimal texture directly (1x1 black)
        unsigned char pixel[4] = {0, 0, 0, 255};
        m_Texture = std::make_unique<Core::Texture>(SDL_PIXELFORMAT_ABGR8888,
                                                   1, 1, pixel);
        m_Size = {1.0F, 1.0F};
    } else {
        m_Texture = std::make_unique<Core::Texture>(
            Core::SdlFormatToGlFormat(surface->format->format),
            surface->pitch / surface->format->BytesPerPixel, surface->h,
            surface->pixels);
        m_Size = {static_cast<float>(surface->pitch /
                                     surface->format->BytesPerPixel),
                  static_cast<float>(surface->h)};
    }
}

void Text::Draw(const Core::Matrices &data) {
    m_UniformBuffer->SetData(0, data);

    m_Texture->Bind(UNIFORM_SURFACE_LOCATION);
    s_Program->Bind();
    s_Program->Validate();

    s_VertexArray->Bind();
    s_VertexArray->DrawTriangles();
}

void Text::InitProgram() {
    // TODO: Create `BaseProgram` from `Program` and pass it into `Drawable`
    s_Program =
        std::make_unique<Core::Program>(PTSD_ASSETS_DIR "/shaders/Base.vert",
                                        PTSD_ASSETS_DIR "/shaders/Base.frag");
    s_Program->Bind();

    GLint location = glGetUniformLocation(s_Program->GetId(), "surface");
    glUniform1i(location, UNIFORM_SURFACE_LOCATION);
}

void Text::InitVertexArray() {
    s_VertexArray = std::make_unique<Core::VertexArray>();

    // NOLINTBEGIN
    // These are vertex data for the rectangle but clang-tidy has magic
    // number warnings

    // Vertex
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            -0.5F, 0.5F,  //
            -0.5F, -0.5F, //
            0.5F, -0.5F,  //
            0.5F, 0.5F,   //
        },
        2));

    // UV
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            0.0F, 0.0F, //
            0.0F, 1.0F, //
            1.0F, 1.0F, //
            1.0F, 0.0F, //
        },
        2));

    // Index
    s_VertexArray->SetIndexBuffer(
        std::make_unique<Core::IndexBuffer>(std::vector<unsigned int>{
            0, 1, 2, //
            0, 2, 3, //
        }));
    // NOLINTEND
}

void Text::ApplyTexture() {
    auto raw_surface = TTF_RenderUTF8_Blended_Wrapped(m_Font.get(),
                                                     m_Text.c_str(),
                                                     m_Color.ToSdlColor(),
                                                     0);

    std::unique_ptr<SDL_Surface, std::function<void(SDL_Surface *)>> surface(
        raw_surface, SDL_FreeSurface);

    if (surface == nullptr) {
        LOG_ERROR("Failed to create text: {}", TTF_GetError());
        // fallback to 1x1 white surface
        SDL_Surface *fallback = SDL_CreateRGBSurfaceWithFormat(
            0, 1, 1, 32, SDL_PIXELFORMAT_ABGR8888);
        if (fallback != nullptr) {
            Uint32 color = SDL_MapRGBA(fallback->format, 255, 255, 255, 255);
            SDL_FillRect(fallback, nullptr, color);
            surface.reset(fallback);
        }
    }

    if (surface != nullptr) {
        m_Texture->UpdateData(Core::SdlFormatToGlFormat(surface->format->format),
                              surface->pitch / surface->format->BytesPerPixel,
                              surface->h, surface->pixels);
        m_Size = {static_cast<float>(surface->pitch /
                                     surface->format->BytesPerPixel),
                  static_cast<float>(surface->h)};
    }
}

std::unique_ptr<Core::Program> Text::s_Program = nullptr;
std::unique_ptr<Core::VertexArray> Text::s_VertexArray = nullptr;

} // namespace Util
