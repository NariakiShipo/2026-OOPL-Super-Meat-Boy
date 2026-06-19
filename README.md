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

- 📄 [Proposal（企劃書）](Proposal.md)
- 📋 [Final Report（期末報告）](2026OOPL_Final.md)

---

## 建置方式

1. Clone 本專案（含子模組）：

   ```bash
   git clone YOUR_GIT_URL --recursive
   ```

2. 建置專案：

   > [!WARNING]
   > 請使用 `Debug` 模式建置，`Release` 路徑目前尚未修正。

   ```sh
   cmake -DCMAKE_BUILD_TYPE=Debug -B build
   cmake --build build
   ```

3. 執行產出的執行檔即可開始遊戲。

---

## 技術架構

本專案採「**框架層 → 遊戲層 → 主控層**」的分層設計，並充分運用 OOP 三大支柱：

- **封裝**：`App` 對外僅暴露 `Start()/Update()/End()` 生命週期
- **繼承**：所有可繪製物件繼承抽象基底 `Core::Drawable`
- **多型**：玩家在不同狀態下動態替換 drawable，渲染端無感知地正確繪製

關卡以 **TMX 地圖檔**驅動，所有數值參數外部化於 **JSON 設定檔**，改數值免重編譯。

> 詳細 OOP 分析請見 [`oop_analysis.md`](oop_analysis.md)

---

*Based on [PTSD framework](https://github.com/ntut-open-source-club/practical-tools-for-simple-design)*
