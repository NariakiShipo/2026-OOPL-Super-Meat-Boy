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
        WORLD_SELECT,
        LEVEL_SELECT,
        UPDATE,
        END,
    };

    // 標題畫面兩階段：PRESS START → 主選單
    enum class TitlePhase {
        PRESS_START,
        MENU,
    };

    // Boss (Lil' Slugger) 狀態機：
    // WAITING(進關延遲) → CHASING(等速追擊) → RUSHING(終點前加速) → CRASHED(撞牆停止)
    enum class BossPhase {
        INACTIVE,
        WAITING,
        CHASING,
        RUSHING,
        CRASHED,
    };

    State GetCurrentState() const { return m_CurrentState; }

    void Start();

    void Title();

    void WorldSelect();

    void LevelSelect();

    void Update();

    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    void LoadGameConfig();
    void InitWorld();
    void LoadLevel(std::size_t levelIndex);
    void ShowTitleScreen();
    void ShowGameplayScreen();
    void ShowWorldSelectScreen();
    void ShowLevelSelectScreen();
    void OpenSettingsMenu();
    void CloseSettingsMenu();
    void StartGame();
    void UpdateTitleScreen();
    void UpdateWorldSelectScreen();
    void UpdateLevelSelectScreen();
    void UpdateSettingsMenu();
    void SetTitlePhase(TitlePhase phase);
    void BuildPauseMenu();
    void OpenPauseMenu();
    void ClosePauseMenu();
    void UpdatePauseMenu();
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
    void ResetLevelDynamics();  // 死亡時重置碎裂磚/飛鋸/發射器/旋轉鋸
    std::shared_ptr<Util::GameObject> CreatePlatform(const glm::vec2 &position,
                                                     const glm::vec2 &size,
                                                     float zIndex,
                                                     const std::string &texturePath,
                                                     const glm::vec4 &uvRect =
                                                         {0.0F, 0.0F, 1.0F, 1.0F},
                                                     bool visible = true) const;
    void UpdateShooters(float dtMs);
    void CheckBuzzsawPlayerCollisions();
    void SpawnBoss();
    void ResetBoss();
    void StepBoss(float dtMs);
    void CheckBossPlayerCollision();
    void SpawnRotors(const std::vector<Game::RotorConfig> &configs);
    void UpdateRotors(float dtMs);
    void CheckRotorPlayerCollisions();

private:
    struct UiTextSpec {
        std::string fontPath = "fonts/BlackOpsOne-Regular.ttf";
        int fontSize = 32;
        std::string text;
        glm::vec2 position = {0.0F, 0.0F};
        float zIndex = 0.0F;
        Util::Color color = Util::Color(255, 255, 255, 255);
    };

    struct LevelSelectConfig {
        std::string fontPath = "fonts/BlackOpsOne-Regular.ttf";
        int tabFontSize = 36;
        int bossFontSize = 34;
        int backFontSize = 32;
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
            "fonts/BlackOpsOne-Regular.ttf",
            40,
            "start game",
            {0.0F, -120.0F},
            130.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec settingsButton{
            "fonts/BlackOpsOne-Regular.ttf",
            40,
            "settings",
            {0.0F, -190.0F},
            130.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec settingsTitle{
            "fonts/BlackOpsOne-Regular.ttf",
            44,
            "settings",
            {0.0F, 130.0F},
            210.0F,
            Util::Color(255, 244, 200, 255),
        };
        UiTextSpec bgmLabel{
            "fonts/BlackOpsOne-Regular.ttf",
            30,
            "bgm",
            {0.0F, 30.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec sfxLabel{
            "fonts/BlackOpsOne-Regular.ttf",
            30,
            "sfx",
            {0.0F, -60.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec backButton{
            "fonts/BlackOpsOne-Regular.ttf",
            36,
            "back",
            {0.0F, -210.0F},
            210.0F,
            Util::Color(255, 255, 255, 255),
        };
        UiTextSpec helpText{
            "fonts/BlackOpsOne-Regular.ttf",
            22,
            "drag the bars to set bgm and sfx volume",
            {0.0F, -270.0F},
            210.0F,
            Util::Color(220, 220, 220, 255),
        };
        UiTextSpec toLevelSelectButton{
            "fonts/BlackOpsOne-Regular.ttf",
            32,
            "back to level select",
            {0.0F, -340.0F},
            210.0F,
            Util::Color(255, 200, 200, 255),
        };
        UiTextSpec statusText{
            "fonts/BlackOpsOne-Regular.ttf",
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
            // 水平加速度模型（原作手感：快起步、放開會滑、空中保留動量）
            float groundAccel = 4000.0F;   // 地面朝輸入方向加速 (px/s^2)
            float groundDecel = 3000.0F;   // 地面無輸入減速（滑行感）
            float airAccel = 2600.0F;      // 空中加速（接近全空控）
            float airDecel = 600.0F;       // 空中無輸入減速（保留動量）
            float jumpVelocity = 500.0F;
            float jumpHoldMaxMs = 170.0F;
            float jumpHoldBoost = 2000.0F;
            float shortHopCutRatio = 0.42F;
            float jumpBufferMs = 90.0F;    // 落地前先按跳的緩衝
            float coyoteMs = 70.0F;        // 離開平台邊緣後仍可跳的寬限
            float wallJumpHorizontalVelocity = 430.0F;
            float wallJumpControlLockMs = 140.0F;
            float wallReattachCooldownMs = 110.0F;
            float wallSlideMaxFallSpeed = 260.0F;
            // 非對稱重力：下落較快（落地乾脆）；gravity 為舊欄位（兩者預設值）
            float gravity = -1800.0F;
            float gravityUp = -1500.0F;    // 上升段
            float gravityDown = -1900.0F;  // 下落段
            float maxFallSpeed = 650.0F;   // 終端速度
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

    struct BuzzsawConfig {
        float speed = 400.0F;                  // px per second
        float intervalMs = 2000.0F;            // firing interval
        float zIndex = 5.0F;
        glm::vec2 size = {0.0F, 0.0F};        // [0,0] = natural animation size
        float spawnGraceMs = 500.0F;          // wall-collision immunity after spawn
        std::vector<std::string> animFrames = {
            "images/buzzsaw2_1.png",
            "images/buzzsaw2_2.png",
            "images/buzzsaw2_3.png",
        };
        std::size_t animIntervalMs = 80;
    };

    struct BossConfig {
        float speed = 280.0F;          // CHASING 水平速度 (px/s)，< 玩家 walk 300
        float rushSpeed = 700.0F;      // RUSHING 衝撞速度 (px/s)
        float startDelayMs = 1000.0F;  // 進關到起追的延遲
        glm::vec2 visualSize = {340.0F, 200.0F}; // 整體 sprite 顯示尺寸（玩家約14px）
        float zIndex = 9.0F;           // 玩家 zIndex=10，Boss 略低
        // 碰撞箱（相對 Boss 物件中心，+x 為前進方向；依 sprite 區塊換算）
        glm::vec2 bodyOffset = {-27.0F, -21.0F};
        glm::vec2 bodySize = {208.0F, 145.0F};
        glm::vec2 sawOffset = {106.0F, -19.0F};  // 前伸鏈鋸（又長又扁）
        glm::vec2 sawSize = {126.0F, 44.0F};
        float behindKillMarginPx = 40.0F;  // 玩家整個落在機身後方超過此距離 → 視為被輾過
        std::vector<std::string> animFrames = {
            "images/lilslugger_1.png",
            "images/lilslugger_2.png",
        };
        std::size_t animIntervalMs = 160;
        std::vector<std::string> crashFrames = {  // CRASHED 撞毀燃燒
            "images/lilslugger_crash.png",
        };
    };

    struct RotorTuningConfig {
        float angularSpeedDegPerSec = 72.0F;  // 旋轉角速度（正值=逆時針）
        float barThickness = 14.0F;           // 旋臂視覺厚度
        float barZIndex = 4.0F;
        float sawZIndex = 5.0F;
        float hitboxScale = 0.85F;            // 鋸片 AABB 縮小係數（圓形視覺的公平判定）
        std::string barTexturePath = "images/rotor_bar.png";
        // 鋸片動畫沿用 buzzsaw.animFrames / animIntervalMs
    };

    struct GameplayConfig {
        AudioConfig audio;
        UiConfig ui;
        LevelSelectConfig levelSelect;
        PlayerConfig player;
        CameraConfig camera;
        CollisionConfig collision;
        BuzzsawConfig buzzsaw;
        BossConfig boss;
        RotorTuningConfig rotor;
        float goalSizeScale = 0.9F;
    };

    struct BreakableBlock {
        std::shared_ptr<Util::GameObject> object;
        std::shared_ptr<Util::Animation> animation;
        glm::vec2 colliderSize = {0.0F, 0.0F};
        bool breaking = false;
        bool broken = false;
    };

    struct LiveBuzzsaw {
        std::shared_ptr<Util::GameObject> object;
        glm::vec2 velocity = {0.0F, 0.0F};
        float graceTimerMs = 0.0F;  // wall-collision immunity countdown
        bool active = true;
    };

    struct ShooterState {
        Game::ShooterConfig config;
        float timerMs = 0.0F;  // countdown; fires when <= 0
    };

    struct BossState {
        std::shared_ptr<Util::GameObject> object;
        BossPhase phase = BossPhase::INACTIVE;
        float waitTimerMs = 0.0F;
        std::shared_ptr<Core::Drawable> walkDrawable;   // 行走動畫
        std::shared_ptr<Core::Drawable> crashDrawable;  // 撞毀燃燒
    };

    struct RotorState {
        Game::RotorConfig config;
        float angleDeg = 0.0F;
        std::shared_ptr<Util::GameObject> bar;   // 旋臂（隨角度旋轉）
        std::shared_ptr<Util::GameObject> sawA;  // 兩端鋸片
        std::shared_ptr<Util::GameObject> sawB;
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
    std::shared_ptr<Util::GameObject> m_SettingsToLevelSelectButton;

    std::vector<std::shared_ptr<Util::GameObject>> m_TitleScreenObjects;
    std::vector<std::shared_ptr<Util::GameObject>> m_SettingsObjects;
    // 設定頁基準 transform（開啟時依相機位置/縮放重新套用）
    std::vector<glm::vec2> m_SettingsBasePositions;
    std::vector<glm::vec2> m_SettingsBaseScales;

    // ── 標題畫面（PRESS START → 主選單）─────────────────────
    TitlePhase m_TitlePhase = TitlePhase::PRESS_START;
    int m_TitleMenuIndex = 0;
    float m_TitleBlinkMs = 0.0F;
    std::shared_ptr<Util::GameObject> m_PressStartPanel;
    std::shared_ptr<Util::GameObject> m_PressStartText;
    std::shared_ptr<Util::GameObject> m_TitleMenuPanel;
    std::shared_ptr<Util::GameObject> m_TitleMenuArrow;
    std::shared_ptr<Util::GameObject> m_TitleHintText;
    std::vector<std::shared_ptr<Util::GameObject>> m_TitleMenuItems;

    // ── 世界地圖選擇 ─────────────────────────────────────────
    std::shared_ptr<Util::GameObject> m_WorldSelTitleText;
    std::shared_ptr<Util::GameObject> m_WorldSelCountText;
    std::shared_ptr<Util::GameObject> m_WorldSelPreview;
    std::vector<std::shared_ptr<Util::GameObject>> m_WorldSelectObjects;

    // ── 關卡節點圖 ───────────────────────────────────────────
    struct LevelNode {
        std::shared_ptr<Util::GameObject> tile;
        std::shared_ptr<Util::GameObject> label;   // BOSS 節點為 nullptr
        std::size_t globalLevelIndex = 0;
        glm::vec2 position = {0.0F, 0.0F};
        std::string displayName;
    };
    std::vector<LevelNode> m_LevelNodes;
    int m_LevelNodeIndex = 0;
    std::shared_ptr<Util::GameObject> m_LevelSelBottomText;

    // ── 暫停選單 ─────────────────────────────────────────────
    bool m_PauseMenuVisible = false;
    int m_PauseMenuIndex = 0;
    std::vector<std::shared_ptr<Util::GameObject>> m_PauseObjects;
    std::vector<std::shared_ptr<Util::GameObject>> m_PauseMenuItems;
    std::shared_ptr<Util::GameObject> m_PauseArrow;
    // 基準 transform（開啟時以相機位置/縮放重新套用，避免重複疊加）
    std::vector<glm::vec2> m_PauseBasePositions;
    std::vector<glm::vec2> m_PauseBaseScales;

    std::shared_ptr<Util::GameObject> m_Player;
    std::shared_ptr<Core::Drawable> m_PlayerIdleDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunLeftDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerRunRightDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerJumpDrawable;
    std::shared_ptr<Core::Drawable> m_PlayerFallDrawable;

    PlayerAnimState m_PlayerAnimState = PlayerAnimState::IDLE;
    bool m_PlayerFacingRight = true;
    // 目前 RUN 貼圖採用的朝向；朝向改變時即使仍在 RUN 也要重貼圖。
    bool m_PlayerRunDrawableFacingRight = true;

    std::vector<std::shared_ptr<Util::GameObject>> m_Platforms;
    std::vector<BreakableBlock> m_BreakableBlocks;
    std::vector<std::shared_ptr<Util::GameObject>> m_DeathZones;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelRenderTiles;
    std::shared_ptr<Util::GameObject> m_GoalFlag;

    std::vector<ShooterState>  m_Shooters;
    std::vector<LiveBuzzsaw>   m_LiveBuzzsaws;

    std::vector<RotorState> m_Rotors;  // 旋轉鋸

    // Boss（由目前關卡的 LevelConfig::boss 驅動）
    BossState m_Boss;
    bool m_LevelHasBoss = false;
    glm::vec2 m_BossSpawnPoint = {0.0F, 0.0F};
    float m_BossRushTriggerX = 0.0F;  // 玩家越過此 x → Boss 進 RUSHING
    float m_BossCrashX = 0.0F;        // Boss 抵達此 x → CRASHED
    std::size_t m_BossTestLevelIndex = 0;  // 除錯入口（選關畫面按 B）

    std::shared_ptr<Util::GameObject> m_StatusBoard;
    std::shared_ptr<Util::Text> m_StatusText;
    // 作弊模式提示字：m_CheatSawImmune 開啟（F2）時顯示於螢幕下方中央。
    std::shared_ptr<Util::GameObject> m_CheatIndicator;

    std::vector<Game::WorldData> m_Worlds;
    std::vector<Game::LevelConfig> m_Levels;
    std::size_t m_CurrentLevelIndex = 0;

    // 關卡選擇畫面
    std::size_t m_LevelSelectWorldIndex = 0;
    std::shared_ptr<Util::GameObject> m_LevelSelectBackground;
    std::vector<std::shared_ptr<Util::GameObject>> m_WorldTabButtons;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelSelectButtons;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelHoverOverlays;
    std::shared_ptr<Util::GameObject> m_LevelSelectBossButton;  // Forest 第9格(BOSS)
    std::shared_ptr<Util::GameObject> m_LevelSelectBackButton;
    std::shared_ptr<Util::GameObject> m_LevelSelectTitle;
    std::vector<std::shared_ptr<Util::GameObject>> m_LevelSelectObjects;

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
    float m_JumpBufferTimerMs = 0.0F;  // 跳躍輸入緩衝倒數
    float m_CoyoteTimerMs = 0.0F;      // 土狼時間倒數
    bool m_PlayerOnGround = false;
    bool m_PlayerOnWall = false;
    float m_WallJumpDirection = 0.0F;
    float m_WallControlLockTimerMs = 0.0F;
    float m_WallReattachCooldownMs = 0.0F;
    float m_BreakableDebugLogCooldownMs = 0.0F;
    bool m_LevelCleared = false;
    int m_RespawnCount = 0;

    // 作弊模式：開啟後碰到鋸類危險物（死亡區動畫鋸 / 飛鋸 / 旋轉鋸）不會死亡。
    // 由 F2 切換，預設關閉。
    bool m_CheatSawImmune = false;
};

#endif
