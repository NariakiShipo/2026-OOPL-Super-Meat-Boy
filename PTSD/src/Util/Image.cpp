#include "Util/Image.hpp"

#include "Util/Logger.hpp"
#include "pch.hpp"

#include "Core/Texture.hpp"
#include "Core/TextureUtils.hpp"

#include "Util/MissingTexture.hpp"
#include "Util/TransformUtils.hpp"

#include "config.hpp"
#include <glm/fwd.hpp>

// Shared GL texture cache: same filepath → same GPU texture object.
// Keyed by absolute filepath; weak_ptr so textures are released when no
// Image refers to them anymore.
static std::unordered_map<std::string, std::weak_ptr<Core::Texture>>
    s_TextureCache;

std::shared_ptr<SDL_Surface> LoadSurface(const std::string &filepath) {
    auto surface = std::shared_ptr<SDL_Surface>(IMG_Load(filepath.c_str()),
                                                SDL_FreeSurface);

    if (surface == nullptr) {
        surface = {GetMissingTextureSDLSurface(), SDL_FreeSurface};
        LOG_ERROR("Failed to load image: '{}'", filepath);
        LOG_ERROR("{}", IMG_GetError());
    }

    return surface;
}

namespace Util {
Image::Image(const std::string &filepath)
    : m_Path(filepath) {
    if (s_Program == nullptr) {
        InitProgram();
    }

    m_UniformBuffer = std::make_unique<Core::UniformBuffer<Core::Matrices>>(
        *s_Program, "Matrices", 0);

    auto surface = s_Store.Get(filepath);

    auto it = s_TextureCache.find(filepath);
    if (it != s_TextureCache.end() && !it->second.expired()) {
        m_Texture = it->second.lock();
    } else {
        m_Texture = std::make_shared<Core::Texture>(
            Core::SdlFormatToGlFormat(surface->format->format), surface->w,
            surface->h, surface->pixels);
        s_TextureCache[filepath] = m_Texture;
    }
    m_TextureSize = {surface->w, surface->h};

    UpdateDisplaySize();
    InitVertexArray();
}

void Image::SetImage(const std::string &filepath) {
    auto surface = s_Store.Get(filepath);

    // Look up the shared texture cache; don't mutate the old texture in-place
    // (that would corrupt any other Image sharing it).
    auto it = s_TextureCache.find(filepath);
    if (it != s_TextureCache.end() && !it->second.expired()) {
        m_Texture = it->second.lock();
    } else {
        m_Texture = std::make_shared<Core::Texture>(
            Core::SdlFormatToGlFormat(surface->format->format), surface->w,
            surface->h, surface->pixels);
        s_TextureCache[filepath] = m_Texture;
    }
    m_Path = filepath;
    m_TextureSize = {surface->w, surface->h};

    UpdateDisplaySize();
    InitVertexArray();
}

void Image::SetUVRect(const glm::vec4 &uvRect) {
    m_UvRect = uvRect;
    UpdateDisplaySize();
    InitVertexArray();
}

void Image::Draw(const Core::Matrices &data) {
    m_UniformBuffer->SetData(0, data);

    m_Texture->Bind(UNIFORM_SURFACE_LOCATION);
    s_Program->Bind();
    s_Program->Validate();

    m_VertexArray->Bind();
    m_VertexArray->DrawTriangles();
}

void Image::InitProgram() {
    // TODO: Create `BaseProgram` from `Program` and pass it into `Drawable`
    s_Program =
        std::make_unique<Core::Program>(PTSD_ASSETS_DIR "/shaders/Base.vert",
                                        PTSD_ASSETS_DIR "/shaders/Base.frag");
    s_Program->Bind();

    GLint location = glGetUniformLocation(s_Program->GetId(), "surface");
    glUniform1i(location, UNIFORM_SURFACE_LOCATION);
}

void Image::InitVertexArray() {
    m_VertexArray = std::make_unique<Core::VertexArray>();

    // NOLINTBEGIN
    // These are vertex data for the rectangle but clang-tidy has magic
    // number warnings

    // Vertex
    m_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            -0.5F, 0.5F,  //
            -0.5F, -0.5F, //
            0.5F, -0.5F,  //
            0.5F, 0.5F,   //
        },
        2));

    // UV
    m_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            m_UvRect.x, m_UvRect.y, //
            m_UvRect.x, m_UvRect.w, //
            m_UvRect.z, m_UvRect.w, //
            m_UvRect.z, m_UvRect.y, //
        },
        2));

    // Index
    m_VertexArray->SetIndexBuffer(
        std::make_unique<Core::IndexBuffer>(std::vector<unsigned int>{
            0, 1, 2, //
            0, 2, 3, //
        }));
    // NOLINTEND
}

void Image::UpdateDisplaySize() {
    const float uWidth = std::max(0.0F, m_UvRect.z - m_UvRect.x);
    const float vHeight = std::max(0.0F, m_UvRect.w - m_UvRect.y);
    m_Size = {m_TextureSize.x * uWidth, m_TextureSize.y * vHeight};
}

std::unique_ptr<Core::Program> Image::s_Program = nullptr;

Util::AssetStore<std::shared_ptr<SDL_Surface>> Image::s_Store(LoadSurface);
} // namespace Util
