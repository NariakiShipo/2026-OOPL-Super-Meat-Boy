# Resources 使用方式分析

本文件整理 `SupermeatBoy/resources` 內所有檔案在專案中的使用情況（直接載入、透過 TMX 間接載入、僅文件用途、未使用）。

## 1. 資源載入機制

### A. 直接以檔案路徑載入（`new File("resources/..."`)）
主要出現在以下類別：
- `src/app/MeatBoyFrame.java`
- `src/character/MeatBoy.java`
- `src/platform/BuzzSaw.java`
- `src/platform/DisappearPlat.java`
- `src/platform/SawShooter.java`

特性：
- 依賴目前工作目錄（working directory）包含 `resources` 資料夾。
- 從 `SupermeatBoy` 目錄執行時可正常找到檔案。

### B. 透過 ClassLoader 載入（地圖與 tileset）
流程：
1. `MeatBoyFrame` 決定關卡檔名（`forest1.tmx` ~ `forest8.tmx`、`factory1.tmx` ~ `factory8.tmx`）。
2. `MeatBoyLevel` 以 `getClassLoader().getResource(fp)` 載入 TMX。
3. `TileSet` 解析 TMX 內 `<tileset><image source="...">`，再以 `getClassLoader().getResource(src)` 載入對應 PNG。

特性：
- TMX 與其引用的 tileset 圖片必須在 classpath 中可見。

## 2. 檔案逐一使用情況（resources 全部）

| 檔案 | 使用情況 | 使用方式 / 位置 |
|---|---|---|
| `bandagegirl.png` | 未使用 | 程式碼與 TMX 皆無引用 |
| `buzzsaw2.png` | 直接使用 | `platform/BuzzSaw` 旋轉鋸齒動畫圖（切 3 幀） |
| `disappearing.png` | 直接使用 | `platform/DisappearPlat` 消失平台動畫圖（切 11 幀） |
| `disappearing2.png` | 未使用 | 程式碼與 TMX 皆無引用 |
| `endtiles03.png` | 間接使用（TMX） | `forest6.tmx` tileset image source |
| `eobstacles.png` | 間接使用（TMX） | factory1/2/3/4/5/6/8、forest8 |
| `factory1.tmx` | 直接指定關卡 | `MeatBoyFrame` 關卡選單 -> `MeatBoyLevel` |
| `factory2.tmx` | 直接指定關卡 | 同上 |
| `factory3.tmx` | 直接指定關卡 | 同上 |
| `factory4.tmx` | 直接指定關卡 | 同上 |
| `factory5.tmx` | 直接指定關卡 | 同上 |
| `factory6.tmx` | 直接指定關卡 | 同上 |
| `factory7.tmx` | 直接指定關卡 | 同上 |
| `factory8.tmx` | 直接指定關卡 | 同上 |
| `factorylevelselect.png` | 直接使用 | `app/MeatBoyFrame` 工廠關卡選單背景 |
| `factoryp01.png` | 間接使用（TMX） | factory1/3/4/7/8 |
| `factoryp02.png` | 間接使用（TMX） | factory2/5 |
| `factoryp03.png` | 間接使用（TMX） | factory6 |
| `factorytiles01.png` | 間接使用（TMX） | factory1~8 |
| `factorytiles02.png` | 間接使用（TMX） | factory1~8 |
| `forest1.tmx` | 直接指定關卡 | `MeatBoyFrame` 關卡選單 -> `MeatBoyLevel` |
| `forest2.tmx` | 直接指定關卡 | 同上 |
| `forest3.tmx` | 直接指定關卡 | 同上 |
| `forest4.tmx` | 直接指定關卡 | 同上 |
| `forest5.tmx` | 直接指定關卡 | 同上 |
| `forest6.tmx` | 直接指定關卡 | 同上 |
| `forest7.tmx` | 直接指定關卡 | 同上 |
| `forest8.tmx` | 直接指定關卡 | 同上 |
| `forestani.png` | 間接使用（TMX） | `forest6.tmx` tileset image source |
| `forestbg.png` | 間接使用（TMX） | forest1~8 |
| `forestbgflipped.png` | 間接使用（TMX） | forest1~5、forest7、forest8 |
| `forestlevelselect.png` | 直接使用 | `app/MeatBoyFrame` 森林關卡選單背景 |
| `forestp01.png` | 未使用 | 程式碼與 TMX 皆無引用 |
| `forestp02.png` | 未使用 | 程式碼與 TMX 皆無引用 |
| `forestset1.png` | 間接使用（TMX） | forest1~7 |
| `foresttiles01.png` | 間接使用（TMX） | forest1~8 |
| `loadingscreen.png` | 直接使用 | `app/MeatBoyFrame` 載入畫面 |
| `meatboystanding.png` | 直接使用 | `app/MeatBoyFrame` icon/游標圖、`character/MeatBoy` 角色站立圖 |
| `monsterts.png` | 未使用 | 程式碼與 TMX 皆無引用 |
| `myimages.png` | 間接使用（TMX） | 所有 forest/factory 關卡都引用 |
| `sawshooter.png` | 直接使用 | `platform/SawShooter`（水平發射器） |
| `sawshooter2.png` | 直接使用 | `platform/SawShooter`（垂直發射器） |
| `screenshot1.png` | 文件用途 | README 展示截圖（GitHub 連結） |
| `screenshot2.png` | 文件用途 | README 展示截圖（GitHub 連結） |
| `screenshot3.png` | 文件用途 | README 展示截圖（GitHub 連結） |
| `screenshot4.png` | 文件用途 | README 展示截圖（GitHub 連結） |
| `sprintLeft.png` | 直接使用 | `character/MeatBoy` 左衝刺貼圖 |
| `sprintRight.png` | 直接使用 | `character/MeatBoy` 右衝刺貼圖 |
| `titlescreen.png` | 直接使用 | `app/MeatBoyFrame` 主選單/控制畫面背景 |

## 3. TMX 中控制遊戲語義的特殊 tile（重要）

除了繪圖用途外，`TileMap.loadMap()` 會對 `stationary` layer 的特定 tile index 做邏輯判斷：
- tile `1`: MeatBoy 起始點
- tile `2`: Bandage Girl（終點）
- tile `3`: Disappearing Platform
- tile `13`~`16`: SawShooter（四個方向）

這代表 `myimages.png` 不只是美術資源，還包含關卡邏輯標記所依賴的 tile 編號配置。

## 4. 目前未使用檔案清單

以下檔案在目前程式與 TMX 中沒有被引用：
- `bandagegirl.png`
- `disappearing2.png`
- `forestp01.png`
- `forestp02.png`
- `monsterts.png`

可視需求決定：
- 保留作為未來素材，或
- 清理以降低資源體積與維護成本。
