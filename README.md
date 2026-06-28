# 🥩 超級肉肉哥 Super Meat Boy

> **2026 OOPL 期末專案｜T21 組｜113AC1012 陳俊佑**

---

## 遊戲簡介

《超級肉肉哥》（Super Meat Boy）是一款以「**精準操作**」與「**極速重生**」聞名的高難度平台遊戲。

玩家扮演一塊方方正正、顏色深紅、猶如肉塊的「**肉肉哥**」（Meat Boy），目標是穿越佈滿電鋸與陷阱的關卡，營救被大反派「**胎兒博士**」（Dr. Fetus）綁架的女朋友「**繃帶妹**」（Bandage Girl）。

本專案以 **C++** 搭配 **PTSD（Practical Tools for Simple Design）** 框架重現原作核心手感，包含：

- ⚡ 靈敏的水平加速與摩擦物理
- 🧱 滑牆（Wall Slide）與牆壁跳（Wall Jump）
- 💀 碰到陷阱「死亡即刻重置、瞬間回到起點」的快節奏循環
- 🩹 繃帶收集品、可解鎖角色、通關評分（A+/A）與過場演出

🎮 [觀看原版遊戲介紹影片](https://youtu.be/snaionoxjos?si=ESloz3greDftgajJ)
🎮 [觀看本次遊戲還原示範影片](https://youtu.be/8s2QsXmRj9Y)

---

## 操作方式

| 按鍵 | 功能 |
|------|------|
| `A` / `D`（或 `←` / `→`） | 左右移動 |
| `Space` / `W` | 跳躍（長按跳更高，短按小跳） |
| 貼牆下落 | 自動滑牆；滑牆時按跳 → 牆壁跳 |
| `Shift` | 衝刺（加速） |
| `R` | 手動重生 |
| `ESC` | 暫停選單 |
| `F2` | 作弊模式：碰到鏈鋸不死（除錯用） |
| `N` | 通關後進入下一關 |

---

## 專案文件

- 📄 [Proposal（企劃書）](https://github.com/NariakiShipo/2026-OOPL-Report/blob/main/Proposal.md)
- 📋 [Final Report（期末報告）](https://github.com/NariakiShipo/2026-OOPL-Report/blob/main/2026OOPL_Final.md)

---

## 建置方式

本專案使用 **CMake** 建置，並透過 `FetchContent` **自動下載所有第三方依賴**（SDL2、SDL2_image/ttf/mixer、glew、glm、spdlog、ImGui、tmxlite、nlohmann/json…）。**不需要**手動安裝這些函式庫，也**不需要** `--recursive`（PTSD 框架已內含於本專案）。

> [!NOTE]
> 第一次建置時 CMake 會下載並編譯上述依賴，需要網路連線，且耗時較久（依網速與機器約數分鐘）。之後的建置會使用快取，速度很快。

### 事前準備

| 工具 | 版本需求 | 說明 |
|------|----------|------|
| CMake | ≥ 3.16 | 已支援 CMake 4.x（內建相容性設定） |
| C++ 編譯器 | 支援 C++17 | macOS：Apple Clang／Windows：MSVC 或 Clang |
| OpenGL | 系統內建 | macOS、Windows 一般免額外安裝 |

### 通用步驟

> [!WARNING]
> 請使用 `Debug` 模式建置，`Release` 路徑目前尚未修正。

```sh
# 1. Clone 本專案（PTSD 已內含，不需要 --recursive）
git clone YOUR_GIT_URL
cd 2026-OOPL-Super-Meat-Boy

# 2. 設定並建置（首次會自動下載依賴）
cmake -DCMAKE_BUILD_TYPE=Debug -B build
cmake --build build
```

### 🍎 macOS

- 安裝 [Xcode Command Line Tools](https://developer.apple.com/xcode/)（提供 Apple Clang）：

  ```sh
  xcode-select --install
  ```

- 安裝 CMake（擇一）：

  ```sh
  brew install cmake
  ```

- 依上方「通用步驟」建置，執行檔產生於 `build/SuperMeatBoy`：

  ```sh
  ./build/SuperMeatBoy
  ```

### 🪟 Windows

- 安裝 [Visual Studio](https://visualstudio.microsoft.com/)（含「**使用 C++ 的桌面開發**」工作負載）與 [CMake](https://cmake.org/download/)（Visual Studio 安裝程式亦可一併勾選）。

- 使用 **x64** 工具鏈建置（建議在「Developer Command Prompt for VS」或 PowerShell 中執行）：

  ```powershell
  cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug -B build
  cmake --build build --config Debug
  ```

- 執行檔產生於 `build\Debug\SuperMeatBoy.exe`：

  ```powershell
  .\build\Debug\SuperMeatBoy.exe
  ```

> [!TIP]
> 若你的 CMake 為 4.x 並在建置 SDL2 等舊版依賴時遇到 `Compatibility with CMake < 3.5 has been removed` 錯誤，本專案已內建 `CMAKE_POLICY_VERSION_MINIMUM=3.5` 處理；如仍有問題，可在 configure 時手動加上 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`。

---

## 技術架構

本專案採「**框架層 → 遊戲層 → 主控層**」的分層設計，並充分運用 OOP 三大支柱：

- **封裝**：`App` 對外僅暴露 `Start()/Update()/End()` 生命週期
- **繼承**：所有可繪製物件繼承抽象基底 `Core::Drawable`
- **多型**：玩家在不同狀態下動態替換 drawable，渲染端無感知地正確繪製

關卡以 **TMX 地圖檔**驅動，所有數值參數外部化於 **JSON 設定檔**，改數值免重編譯。

---

*Based on [PTSD framework](https://github.com/ntut-open-source-club/practical-tools-for-simple-design)*
