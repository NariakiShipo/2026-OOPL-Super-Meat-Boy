#include "Util/TransformUtils.hpp"

#include "config.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>

namespace Util {
namespace {
glm::vec2 s_CameraPosition = {0.0F, 0.0F};
float s_CameraZoom = 1.0F;
}

Core::Matrices ConvertToUniformBufferData(const Util::Transform &transform,
                                          const glm::vec2 &size,
                                          const float zIndex) {
    constexpr glm::mat4 eye(1.F);

    constexpr float nearClip = -1000;
    constexpr float farClip = 1000;

    const float zoom = (zIndex >= 100.0F) ? 1.0F : std::max(s_CameraZoom, 0.0001F);
    const float virtualWindowWidth = WINDOW_WIDTH / zoom;
    const float virtualWindowHeight = WINDOW_HEIGHT / zoom;

    auto projection =
        glm::ortho<float>(0.0F, 1.0F, 0.0F, 1.0F, nearClip, farClip);
    auto view =
        glm::scale(eye, {1.F / virtualWindowWidth, 1.F / virtualWindowHeight, 1.F}) *
        glm::translate(eye, {virtualWindowWidth / 2, virtualWindowHeight / 2, 0});

    // TODO: TRS comment
    auto model = glm::translate(eye, {transform.translation - s_CameraPosition,
                                      zIndex}) *
                 glm::rotate(eye, transform.rotation, glm::vec3(0, 0, 1)) *
                 glm::scale(eye, {transform.scale * size, 1});

    Core::Matrices data = {
        model,
        projection * view,
    };

    return data;
}

void SetCameraPosition(const glm::vec2 &position) { s_CameraPosition = position; }

void SetCameraZoom(const float zoom) { s_CameraZoom = zoom; }

glm::vec2 GetCameraPosition() { return s_CameraPosition; }

float GetCameraZoom() { return s_CameraZoom; }

} // namespace Util
