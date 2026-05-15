#include "App.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "common/AssetPath.hpp"

#include "Util/Logger.hpp"

namespace {
using json = nlohmann::json;

constexpr const char *kGameplayConfigPath = "config/gameplay.json";

glm::vec2 ParseVec2(const json &value, const glm::vec2 &fallback) {
    if (!value.is_array() || value.size() < 2) {
        return fallback;
    }
    return {value[0].get<float>(), value[1].get<float>()};
}

Util::Color ParseColor(const json &value, const Util::Color &fallback) {
    if (!value.is_array() || value.size() < 3) {
        return fallback;
    }
    const int r = value[0].get<int>();
    const int g = value[1].get<int>();
    const int b = value[2].get<int>();
    const int a = (value.size() >= 4) ? value[3].get<int>()
                                      : static_cast<int>(fallback.a);
    return Util::Color(r, g, b, a);
}
} // namespace

void App::LoadGameConfig() {
    m_Config = GameplayConfig{};

    const std::string resolvedPath = Common::ResolveAssetPath(kGameplayConfigPath);
    std::ifstream input(resolvedPath);
    if (!input.is_open()) {
        m_AudioSettings.bgmVolume = m_Config.audio.defaultBgmVolume;
        m_AudioSettings.sfxVolume = m_Config.audio.defaultSfxVolume;
        m_CameraDeadZoneHalfWidth = m_Config.camera.deadZoneHalfWidth;
        m_CameraDeadZoneHalfHeight = m_Config.camera.deadZoneHalfHeight;
        m_CameraFollowSpeed = m_Config.camera.followSpeed;
        m_CameraLookaheadTime = m_Config.camera.lookaheadTime;
        m_CameraLookaheadResponse = m_Config.camera.lookaheadResponse;
        m_CameraLookaheadMaxX = m_Config.camera.lookaheadMaxX;
        m_CameraLookaheadMaxUp = m_Config.camera.lookaheadMaxUp;
        m_CameraLookaheadMaxDown = m_Config.camera.lookaheadMaxDown;
        return;
    }

    try {
        json root;
        input >> root;

        if (root.contains("audio") && root["audio"].is_object()) {
            const auto &audio = root["audio"];
            m_Config.audio.titleBgmPath =
                audio.value("titleBgmPath", m_Config.audio.titleBgmPath);
            m_Config.audio.buttonSfxPath =
                audio.value("buttonSfxPath", m_Config.audio.buttonSfxPath);
            m_Config.audio.defaultBgmVolume =
                audio.value("defaultBgmVolume", m_Config.audio.defaultBgmVolume);
            m_Config.audio.defaultSfxVolume =
                audio.value("defaultSfxVolume", m_Config.audio.defaultSfxVolume);
            m_Config.audio.maxVolume = audio.value("maxVolume", m_Config.audio.maxVolume);
            m_Config.audio.sliderBarWidth =
                audio.value("sliderBarWidth", m_Config.audio.sliderBarWidth);
        }

        if (root.contains("ui") && root["ui"].is_object()) {
            const auto &ui = root["ui"];
            m_Config.ui.titleBackgroundPath =
                ui.value("titleBackgroundPath", m_Config.ui.titleBackgroundPath);
            m_Config.ui.titleBackgroundZ =
                ui.value("titleBackgroundZ", m_Config.ui.titleBackgroundZ);

            if (ui.contains("buttonColors") && ui["buttonColors"].is_object()) {
                const auto &colors = ui["buttonColors"];
                m_Config.ui.buttonDefaultColor =
                    ParseColor(colors.value("default", json::array()),
                               m_Config.ui.buttonDefaultColor);
                m_Config.ui.buttonHoverColor =
                    ParseColor(colors.value("hover", json::array()),
                               m_Config.ui.buttonHoverColor);
            }

            auto parseTextSpec = [&](const json &value, UiTextSpec *out) {
                if (out == nullptr || !value.is_object()) {
                    return;
                }
                out->fontPath = value.value("fontPath", out->fontPath);
                out->fontSize = value.value("fontSize", out->fontSize);
                out->text = value.value("text", out->text);
                out->position =
                    ParseVec2(value.value("position", json::array()), out->position);
                out->zIndex = value.value("zIndex", out->zIndex);
                out->color =
                    ParseColor(value.value("color", json::array()), out->color);
            };

            if (ui.contains("startButton")) {
                parseTextSpec(ui["startButton"], &m_Config.ui.startButton);
            }
            if (ui.contains("settingsButton")) {
                parseTextSpec(ui["settingsButton"], &m_Config.ui.settingsButton);
            }
            if (ui.contains("settingsTitle")) {
                parseTextSpec(ui["settingsTitle"], &m_Config.ui.settingsTitle);
            }
            if (ui.contains("bgmLabel")) {
                parseTextSpec(ui["bgmLabel"], &m_Config.ui.bgmLabel);
            }
            if (ui.contains("sfxLabel")) {
                parseTextSpec(ui["sfxLabel"], &m_Config.ui.sfxLabel);
            }
            if (ui.contains("backButton")) {
                parseTextSpec(ui["backButton"], &m_Config.ui.backButton);
            }
            if (ui.contains("helpText")) {
                parseTextSpec(ui["helpText"], &m_Config.ui.helpText);
            }
            if (ui.contains("statusText")) {
                parseTextSpec(ui["statusText"], &m_Config.ui.statusText);
            }

            m_Config.ui.statusOffset =
                ParseVec2(ui.value("statusOffset", json::array()),
                          m_Config.ui.statusOffset);
            m_Config.ui.levelClearText =
                ui.value("levelClearText", m_Config.ui.levelClearText);
        }

        if (root.contains("player") && root["player"].is_object()) {
            const auto &player = root["player"];
            m_Config.player.idleSpritePath =
                player.value("idleSpritePath", m_Config.player.idleSpritePath);
            m_Config.player.runLeftSpritePath =
                player.value("runLeftSpritePath", m_Config.player.runLeftSpritePath);
            m_Config.player.runRightSpritePath =
                player.value("runRightSpritePath", m_Config.player.runRightSpritePath);
            m_Config.player.scale = player.value("scale", m_Config.player.scale);
            m_Config.player.zIndex = player.value("zIndex", m_Config.player.zIndex);

            if (player.contains("movement") && player["movement"].is_object()) {
                const auto &movement = player["movement"];
                m_Config.player.movement.moveSpeed =
                    movement.value("moveSpeed", m_Config.player.movement.moveSpeed);
                m_Config.player.movement.sprintMultiplier = movement.value(
                    "sprintMultiplier", m_Config.player.movement.sprintMultiplier);
                m_Config.player.movement.jumpVelocity =
                    movement.value("jumpVelocity", m_Config.player.movement.jumpVelocity);
                m_Config.player.movement.jumpHoldMaxMs = movement.value(
                    "jumpHoldMaxMs", m_Config.player.movement.jumpHoldMaxMs);
                m_Config.player.movement.jumpHoldBoost =
                    movement.value("jumpHoldBoost", m_Config.player.movement.jumpHoldBoost);
                m_Config.player.movement.shortHopCutRatio = movement.value(
                    "shortHopCutRatio", m_Config.player.movement.shortHopCutRatio);
                m_Config.player.movement.wallJumpHorizontalVelocity =
                    movement.value("wallJumpHorizontalVelocity",
                                   m_Config.player.movement.wallJumpHorizontalVelocity);
                m_Config.player.movement.wallJumpControlLockMs =
                    movement.value("wallJumpControlLockMs",
                                   m_Config.player.movement.wallJumpControlLockMs);
                m_Config.player.movement.wallReattachCooldownMs =
                    movement.value("wallReattachCooldownMs",
                                   m_Config.player.movement.wallReattachCooldownMs);
                m_Config.player.movement.wallSlideMaxFallSpeed =
                    movement.value("wallSlideMaxFallSpeed",
                                   m_Config.player.movement.wallSlideMaxFallSpeed);
                m_Config.player.movement.gravity =
                    movement.value("gravity", m_Config.player.movement.gravity);
            }

            if (player.contains("animation") && player["animation"].is_object()) {
                const auto &animation = player["animation"];
                m_Config.player.animation.airborneThreshold = animation.value(
                    "airborneThreshold", m_Config.player.animation.airborneThreshold);
                m_Config.player.animation.runThreshold =
                    animation.value("runThreshold", m_Config.player.animation.runThreshold);
            }
        }

        if (root.contains("camera") && root["camera"].is_object()) {
            const auto &camera = root["camera"];
            m_Config.camera.deadZoneHalfWidth =
                camera.value("deadZoneHalfWidth", m_Config.camera.deadZoneHalfWidth);
            m_Config.camera.deadZoneHalfHeight =
                camera.value("deadZoneHalfHeight", m_Config.camera.deadZoneHalfHeight);
            m_Config.camera.followSpeed =
                camera.value("followSpeed", m_Config.camera.followSpeed);
            m_Config.camera.lookaheadTime =
                camera.value("lookaheadTime", m_Config.camera.lookaheadTime);
            m_Config.camera.lookaheadResponse =
                camera.value("lookaheadResponse", m_Config.camera.lookaheadResponse);
            m_Config.camera.lookaheadMaxX =
                camera.value("lookaheadMaxX", m_Config.camera.lookaheadMaxX);
            m_Config.camera.lookaheadMaxUp =
                camera.value("lookaheadMaxUp", m_Config.camera.lookaheadMaxUp);
            m_Config.camera.lookaheadMaxDown =
                camera.value("lookaheadMaxDown", m_Config.camera.lookaheadMaxDown);
            m_Config.camera.minTravelX =
                camera.value("minTravelX", m_Config.camera.minTravelX);
            m_Config.camera.minTravelY =
                camera.value("minTravelY", m_Config.camera.minTravelY);
            m_Config.camera.zoomOutFactor =
                camera.value("zoomOutFactor", m_Config.camera.zoomOutFactor);
        }

        if (root.contains("collision") && root["collision"].is_object()) {
            const auto &collision = root["collision"];
            m_Config.collision.wallBounceMinSpeed = collision.value(
                "wallBounceMinSpeed", m_Config.collision.wallBounceMinSpeed);
            m_Config.collision.wallBounceDamping = collision.value(
                "wallBounceDamping", m_Config.collision.wallBounceDamping);
            m_Config.collision.breakableTopContactEpsilon = collision.value(
                "breakableTopContactEpsilon",
                m_Config.collision.breakableTopContactEpsilon);
        }

        m_Config.goalSizeScale = root.value("goalSizeScale", m_Config.goalSizeScale);
    } catch (const std::exception &e) {
        LOG_WARN("Failed to parse gameplay config '{}': {}", resolvedPath, e.what());
    }

    m_AudioSettings.bgmVolume = m_Config.audio.defaultBgmVolume;
    m_AudioSettings.sfxVolume = m_Config.audio.defaultSfxVolume;
    m_CameraDeadZoneHalfWidth = m_Config.camera.deadZoneHalfWidth;
    m_CameraDeadZoneHalfHeight = m_Config.camera.deadZoneHalfHeight;
    m_CameraFollowSpeed = m_Config.camera.followSpeed;
    m_CameraLookaheadTime = m_Config.camera.lookaheadTime;
    m_CameraLookaheadResponse = m_Config.camera.lookaheadResponse;
    m_CameraLookaheadMaxX = m_Config.camera.lookaheadMaxX;
    m_CameraLookaheadMaxUp = m_Config.camera.lookaheadMaxUp;
    m_CameraLookaheadMaxDown = m_Config.camera.lookaheadMaxDown;
}
