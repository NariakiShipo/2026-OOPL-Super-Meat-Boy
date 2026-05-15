#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include <filesystem>

#include "Core/Drawable.hpp"

#include "game/LevelData.hpp"

#include "Util/Color.hpp"
#include "Util/GameObject.hpp"
#include "Util/Animation.hpp"
#include "Util/Renderer.hpp"
#include "Util/Text.hpp"

namespace Util {
class BGM;
class SFX;
} // namespace Util

class App {
public:
    enum class PlayerAnimState {
        IDLE,
        RUN,
        JUMP,
        FALL,
    };

    enum class State {
        START,
        TITLE,
        UPDATE,
        END,
    };

    State GetCurrentState() const { return m_CurrentState; }

    void Start();

    void Title();

    void Update();

    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    void LoadGameConfig();
    void InitWorld();
    void LoadLevel(std::size_t levelIndex);
    void ShowTitleScreen();
    void ShowGameplayScreen();
    void OpenSettingsMenu();
    void CloseSettingsMenu();
    void StartGame();
    void UpdateTitleScreen();
    void UpdateSettingsMenu();
    void LoadAudioSettings();
    void SaveAudioSettings() const;
    void ApplyAudioSettings();
    void RefreshSettingsText();
    void StepPlayer(float dtMs);
    void UpdateCamera(float dtMs);
    void ResolvePlayerPlatformCollisions(const glm::vec2 &previousPosition);
    void UpdatePlayerAnimation(float moveAxis);
    void UpdateBreakableBlocks();
    void ApplyPlayerDrawable(const std::shared_ptr<Core::Drawable> &drawable);
    void RespawnPlayer();
    std::shared_ptr<Util::GameObject> CreatePlatform(const glm::vec2 &position,
                                                     const glm::vec2 &size,
                                                     float zIndex,
                                                     const std::string &texturePath,
                                                     const glm::vec4 &uvRect =
                                                         {0.0F, 0.0F, 1.0F, 1.0F},
                                                     bool visible = true) const;

private:
    struct UiTextSpec {
        std::string fontPath = "fonts/Inter.ttf";
        int fontSize = 32;
        std::string text;
        glm::vec2 position = {0.0F, 0.0F};
        float zIndex = 0.0F;
        Util::Color color = Util::Color(255, 255, 255, 255);
    };

    struct AudioConfig {
        std::string titleBgmPath = "audio/testbgm.mp3";
        std::string buttonSfxPath = "audio/Click.wav";
        int defaultBgmVolume = 100;
        int defaultSfxVolume = 100;
        int maxVolume = 128;
        int sliderBarWidth = 20;
    };

    struct UiConfig {
        std::string titleBackgroundPath = "images/titlescreen.png";
        float titleBackgroundZ = -100.0F;
        Util::Color buttonDefaultColor = Util::Color(255, 255, 255, 255);
        Util::Color buttonHoverColor = Util::Color(255, 244, 200, 255);
        UiTextSpec startButton{
            "fonts/Inter.ttf",
            40,
            "start game",
            {0.0F, -120.0F},
            130.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec settingsButton{
            "fonts/Inter.ttf",
            40,
            "settings",
            {0.0F, -190.0F},
            130.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec settingsTitle{
            "fonts/Inter.ttf",
            44,
            "settings",
            {0.0F, 130.0F},
            210.0F,
            Util::Color(255, 244, 200, 255),
        };
        UiTextSpec bgmLabel{
            "fonts/Inter.ttf",
            30,
            "bgm",
            {0.0F, 30.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec sfxLabel{
            "fonts/Inter.ttf",
            30,
            "sfx",
            {0.0F, -60.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec backButton{
            "fonts/Inter.ttf",
            36,
            "back",
            {0.0F, -210.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec helpText{
            "fonts/Inter.ttf",
            22,
            "drag the bars to set bgm and sfx volume",
            {0.0F, -270.0F},
            210.0F,
            Util::Color(220, 220, 220, 255),
        };
        UiTextSpec statusText{
            "fonts/Inter.ttf",
            32,
            "Ready",
            {0.0F, 0.0F},
            100.0F,
            Util::Color(255, 255, 255, 255),
        };
        glm::vec2 statusOffset = {-500.0F, 300.0F};
        std::string levelClearText = "LEVEL CLEAR! \nPress N for next level.";
    };

    struct PlayerConfig {
        std::string idleSpritePath = "images/meatboystanding.png";
        std::string runLeftSpritePath = "images/sprintLeft.png";
        std::string runRightSpritePath = "images/sprintRight.png";
        float scale = 0.8F;
        float zIndex = 10.0F;
        struct Movement {
            float moveSpeed = 360.0F;
            float sprintMultiplier = 1.8F;
            float jumpVelocity = 500.0F;
            float jumpHoldMaxMs = 170.0F;
            float jumpHoldBoost = 2000.0F;
            float shortHopCutRatio = 0.42F;
            float wallJumpHorizontalVelocity = 430.0F;
            float wallJumpControlLockMs = 140.0F;
            float wallReattachCooldownMs = 110.0F;
            float wallSlideMaxFallSpeed = 260.0F;
            float gravity = -1800.0F;
        } movement;
        struct Animation {
            float airborneThreshold = 20.0F;
            float runThreshold = 1.0F;
        } animation;
    };

    struct CameraConfig {
        float deadZoneHalfWidth = 120.0F;
        float deadZoneHalfHeight = 72.0F;
        float followSpeed = 9.0F;
        float lookaheadTime = 0.30F;
        float lookaheadResponse = 12.0F;
        float lookaheadMaxX = 300.0F;
        float lookaheadMaxUp = 90.0F;
        float lookaheadMaxDown = 340.0F;
        float minTravelX = 140.0F;
        float minTravelY = 90.0F;
        float zoomOutFactor = 0.7F;
    };

    struct CollisionConfig {
        float wallBounceMinSpeed = 100.0F;
        float wallBounceDamping = 0.22F;
        float breakableTopContactEpsilon = 0.5F;
    };

    struct GameplayConfig {
        AudioConfig audio;
        UiConfig ui;
        PlayerConfig player;
        CameraConfig camera;
        CollisionConfig collision;
        float goalSizeScale = 0.9F;
    };

    struct BreakableBlock {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Animation> animation;
        glm::vec2 colliderSize = {0.0F, 0.0F};
        bool breaking = false;
        bool broken = false;
    };

    struct AudioSettings {
        int bgmVolume = 100;
        int sfxVolume = 100;
    };

    State m_CurrentState = State::START;

    Util::Renderer m_Root;

    std::shared_ptr<Util::GameObject> m_TitleBackground;
    std::shared_ptr<Util::GameObject> m_TitleHeaderText;
    std::shared_ptr<Util::GameObject> m_StartButton;
    std::shared_ptr<Util::GameObject> m_SettingsButton;

    std::shared_ptr<Util::GameObject> m_SettingsTitleText;
    std::shared_ptr<Util::GameObject> m_BgmSettingText;
    std::shared_ptr<Util::GameObject> m_SfxSettingText;
    std::shared_ptr<Util::GameObject> m_SettingsBackButton;
    std::shared_ptr<Util::GameObject> m_SettingsHelpText;

    std::vector<std::shared_ptr<Util::GameObject>> m_TitleScreenObjects;
    std::vector<std::shared_ptr<Util::GameObject>> m_SettingsObjects;

    std::shared_ptr<Util::GameObject> m_Player;
    std::shared_ptr<Core::Drawable> m_PlayerIdleDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunLeftDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunRightDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerJumpDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerFallDrawable;

    PlayerAnimState m_PlayerAnimState = PlayerAnimState::IDLE;
    bool m_PlayerFacingRight = true;

    std::vector<std::shared_ptr<Util::GameObject>> m_Platforms;
    std::vector<BreakableBlock> m_BreakableBlocks;
    std::vector<std::shared_ptr<Util::GameObject>> m_DeathZones;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelRenderTiles;
    std::shared_ptr<Util::GameObject> m_GoalFlag;

    std::shared_ptr<Util::GameObject> m_StatusBoard;
    std::shared_ptr<Util::Text> m_StatusText;

    std::vector<Game::LevelConfig> m_Levels;
    std::size_t m_CurrentLevelIndex = 0;

    GameplayConfig m_Config;

    AudioSettings m_AudioSettings;
    std::filesystem::path m_AudioSettingsPath = "audio_settings.txt";
    std::shared_ptr<Util::BGM> m_TitleBGM;
    std::shared_ptr<Util::SFX> m_ButtonSFX;
    bool m_SettingsMenuVisible = false;

    glm::vec2 m_CameraPosition = {0.0F, 0.0F};
    glm::vec2 m_CameraBoundsMin = {0.0F, 0.0F};
    glm::vec2 m_CameraBoundsMax = {0.0F, 0.0F};
    glm::vec2 m_CameraLookaheadOffset = {0.0F, 0.0F};
    float m_CameraZoom = 1.0F;

    float m_CameraDeadZoneHalfWidth = 120.0F;
    float m_CameraDeadZoneHalfHeight = 72.0F;
    float m_CameraFollowSpeed = 9.0F;
    float m_CameraLookaheadTime = 0.30F;
    float m_CameraLookaheadResponse = 12.0F;
    float m_CameraLookaheadMaxX = 300.0F;
    float m_CameraLookaheadMaxUp = 90.0F;
    float m_CameraLookaheadMaxDown = 340.0F;

    glm::vec2 m_PlayerSpawn = {-520.0F, -40.0F};
    glm::vec2 m_WorldBoundsMin = {-640.0F, -480.0F};
    glm::vec2 m_WorldBoundsMax = {640.0F, 480.0F};
    glm::vec2 m_PlayerColliderSize = {96.0F, 96.0F};
    glm::vec2 m_PlayerVelocity = {0.0F, 0.0F};
    bool m_IsJumping = false;
    float m_JumpHoldTimerMs = 0.0F;
    bool m_PlayerOnGround = false;
    bool m_PlayerOnWall = false;
    float m_WallJumpDirection = 0.0F;
    float m_WallControlLockTimerMs = 0.0F;
    float m_WallReattachCooldownMs = 0.0F;
    float m_BreakableDebugLogCooldownMs = 0.0F;
    bool m_LevelCleared = false;
    int m_RespawnCount = 0;
};

#endif
