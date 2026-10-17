# Leaves & Stone 验收矩阵

状态只有三种：`未验证`、`通过`、`失败`。只有在本机实际执行并留下证据时才能改成后两者；
CI 成功、文件存在、编译通过都不构成某一行的证据。

最近一次更新：2026-10-17（第三轮：全仓库编码审计与中文损坏修复、行尾收口；仍未进行任何实际安装或视觉验收）。

## 第五轮：原图到位与素材溯源核实

原图已放入仓库：`docs/assets/visual-reference.png`，941×1672，2,391,511 字节，
SHA-256 `92755AA5821C1164F30B0ABCA0405202B4514162A334202C6A75BEEB3F720655`。
画面即交接文档描述的暖白底建筑植物插画，文字 BETWEEN LEAVES AND STONE。

### 六张素材确实由这张图派生（已证实）

用这张原图重跑 `scripts/generate-theme-assets.ps1`（输出到临时目录，未覆盖已提交文件），
产出与仓库内六张 PNG **逐字节一致**：

| 素材 | SHA-256（前 16 位） | 与提交版相同 |
| --- | --- | --- |
| architectural-lines.png | CA2246FA42D034CA | 是 |
| building-ivory.png | 87999C064BFEE8D1 | 是 |
| building-terracotta.png | C78698C4F02BC901 | 是 |
| leaf-left.png | A84F25C097D4AD08 | 是 |
| leaf-right.png | 00E726B677119EAC | 是 |
| sky-blue.png | 8313ABA508BC1113 | 是 |

结论：素材来源正确、生成器可复现、已提交素材不是过期产物。生成器还会把
`y=830..925 且 x<480` 的标题带抠成透明，所以候选窗里不会出现海报标题。

目视抽查：`leaf-left.png` 是干净的透明叶片抠图；`architectural-lines.png` 是墨色建筑线稿。

### 新发现：竖向排布时前层叶片会压住候选文字

`DrawFrontArtwork` 在候选卡**之后**绘制（设计意图如此），叶片尺寸取窗口高度的 38%：

- 横向（1 行）：窗口高约 190px → 叶片高约 72px、宽约 78px，首卡从 y=66 起，重叠约 6px，轻微。
- **竖向（5 卡）**：窗口高约 494px → 叶片高约 188px、宽约 203px，首卡从 y=66 起、卡高 64px，
  于是叶片覆盖第 1、2 张卡的大部分左边区域，而文字从 `face.X+29` 开始——**会压在候选文字上**，
  与“装饰不能挡候选文字”直接冲突。

两条修法（都需实机渲染判断，未实施）：

1. 把前层叶片改到候选卡之前绘制，让卡片盖住叶片；约束必然满足，代价是叶片不再与卡片交叠。
2. 保留前后关系，但把叶片裁剪到候选列之外（按候选卡左右边界设 HDC 裁剪区）。

这是视觉取舍，不是纯技术选择，所以留给你定。

## 第四轮：候选窗接线核对

| 项目 | 结论 | 依据 |
| --- | --- | --- |
| 场景渲染是否真的接进候选窗 | 是。`WeaselPanel` 持有 `SceneRenderer` / `ThemeAssets`，绘制、尺寸、定位、命中均走 `m_leaves_scene` | `WeaselPanel.cpp:106,114,1395` |
| 主题 PNG 是否被编进二进制 | 是。`WeaselServer/LeavesStoneAssets.rc` 以 `RCDATA` 嵌入六个 `IDR_LS_*`，且 `WeaselServer.vcxproj` 编译该 rc | `WeaselServer.vcxproj:514`、`resource.h:15-20` |
| 资源从哪个模块取 | `ThemeAssets::Load(GetModuleHandleW(nullptr))`。`WeaselUI` 是**静态库**，候选窗代码最终链进 `WeaselServer.exe`，与资源同模块，取得到 | `WeaselUI.vcxproj:45`、`SceneRenderer` 调用点 |
| 鼠标命中坐标换算 | 正确。客户端坐标 + `window_bounds` 原点得到屏幕坐标，与布局的绝对坐标同一坐标系 | `WeaselPanel.cpp:373-376,427-430` |
| 回调签名 | 正确。`void(size_t* commit, size_t* select, bool* next_page, bool*)`；抬起提交、按下选择/翻页，与上游用法一致 | `WeaselUI.h:70-79`、`WeaselPanel.cpp:377-442` |
| 命中顺序 | 先候选卡后翻页按钮；禁用按钮不参与命中 | `SceneLayout.cpp` `HitTest` |

### 新发现的功能缺陷：7 / 9 候选不生效

设置的候选数是 5 / 7 / 9，但引擎每页候选数由配置决定，两处都写死 5：

- `output/data/default.yaml:10` `page_size: 5`
- `output/data/leaves_stone.schema.yaml:50` `page_size: 5`
- `SceneLayout.cpp:45` 只按 `min(candidates.size(), page_size)` 排版

保存设置只写到 `leaves_stone_settings.yaml`（`SettingsStore::Save`），**没有任何回写引擎或触发部署的路径**；
`SettingsDialog` 也只在面板侧更新主题。所以选 7 或 9 时，界面仍只会出现 5 张候选卡，翻页也按 5 走。

修法（需编译验证，未实施）：

1. 保存设置时把 `menu/page_size` 写进用户目录的补丁配置（`%APPDATA%\LeavesAndStone\Rime\leaves_stone.custom.yaml`），
   再通过已有的 `deploy` 消息让服务端重新部署；这是 Rime 的正规做法。
2. 若首版不打算做动态页大小，则应把设置里的 7 / 9 去掉，避免给出无效选项。

两条路都需要改 C++ 并实机验证，属于**未验证改动**，故本轮只记录不动手。

## 第三轮：中文编码损坏审计

交接文档提到“仓库读取结果出现中文编码异常”。审计结果比预期严重：**10 个文件、共 3200 余处 Unicode 替换符
（U+FFFD）**，即中文已经被不可逆地替换掉，而不是显示问题。成因是 UTF-8 文本被当作 GBK 解码后再存回 UTF-8。

| 文件 | 替换符数 | 修复方式 |
| --- | --- | --- |
| `docs/superpowers/specs/2026-10-16-leaves-and-stone-ime-design.md` | 2950 | 用交接包内的可读副本整体替换（已逐行比对，仅中文行不同） |
| `output/install.nsi` | 390 | 取 Weasel 0.17.4 原文，按 ASCII 骨架逐行对齐恢复；产品自有改动全部保留 |
| `output/data/default.yaml` | 68 | 标点表按 `rime-prelude/punctuation.yaml` 与 Weasel 历史内联表的重排序重建 |
| `WeaselSetup/imesetup.cpp` | 68 | 同 `install.nsi`，上游原文对齐恢复 |
| `output/data/leaves_stone.schema.yaml` | 27 | 方案名与开关文案恢复；`ü` 转换规则原文恢复 |
| `WeaselTSF/LanguageBar.cpp` | 25 | 上游原文对齐恢复（简体/繁体两条提示） |
| `WeaselDeployer/SwitcherSettingsDialog.cpp` | 19 | 上游原文对齐恢复 |
| `include/WeaselUtility.h` | 18 | 上游原文对齐恢复 |
| `docs/superpowers/plans/2026-10-16-leaves-and-stone-ime.md` | 16 | 同设计文档，用包内副本替换 |
| `get-rime.ps1` | 9 | 上游原文对齐恢复 |

修复后全仓复检为 **0 处**，`default.yaml`、`leaves_stone.schema.yaml`、`leaves_stone_theme.yaml`、
`config/product.json` 均可正常解析，`get-rime.ps1` 语法检查通过，6 个合约测试仍全部 PASS。

### 需要人工确认的恢复项

这几处原文有多个合法候选，是按“权威表首项 + 一致性”推定的，不是逐字节还原：

1. `default.yaml` 的半角标点：`[` → `「`、`]` → `」`、`{` → `『`、`}` → `』`。Rime 权威表与 Weasel 历史表都把它们列在首位，故据此恢复；若产品要面向简体排版，可能应改为 `【】`。
2. `leaves_stone.schema.yaml` 的开关文案：`[ 中文, English ]`、`[ 繁体, 简体 ]`、`[ 半角, 全角 ]`、`[ 中文标点, English Punctuation ]`，措辞需产品确认。
3. 上述恢复保留的是**上游原文**，`output/install.nsi` 里仍是「【小狼毫】用戶文件夾」等小狼毫文案，产品改名尚未做。

### 行尾问题（顺手收口）

仓库原先没有 `.gitattributes`，且 `output/install.nsi` 在索引里是**混合行尾**，导致任何单行改动都显示成整文件重写，
审查时无法看出真实改动量。已添加 `.gitattributes`（`text=auto`，`.nsi`/`.nsh`/工程文件固定 CRLF）。

要让历史也干净，需要一次性的 `git add --renormalize .` 提交；这一步会改动大量文件的索引行尾，属于机械变更，
**未擅自执行**，留给带提交权限的人决定。在那之前，看差异请用 `git diff --ignore-cr-at-eol`。

## 第二轮追加核实

| 项目 | 结论 | 依据 |
| --- | --- | --- |
| 6 个 PowerShell 合约测试 | 本机实跑全部 PASS（需 `-ExecutionPolicy Bypass`，本机策略为 AllSigned） | `test\Test*.ps1` |
| 合约测试的强度 | **弱**。`TestRendererContract` / `TestSceneContract` 只检查文件存在与字符串打点，把实现掏空、只留字符串仍会 PASS。不能当作“渲染正确”的证据 | 两个测试源文件全文 |
| `has_visible_shared_background` | 布局层无条件赋 `false`，测试再断言它为 `false`，属同义反复，不构成“无统一大底板”的证据 | `SceneLayout.cpp:40`、`SceneLayoutTests.cpp:60` |
| 候选卡绘制方式 | 逐张独立绘制：每张各自 face + 侧面厚度 + 三层阴影，绘制前先把整面清成全透明；无统一底板/外框。这一条实现与要求一致（仍需目视确认） | `SceneRenderer.cpp` |
| 选中项颜色 | 原为薄荷绿 `(224,239,235)`，与“选中浅蓝”冲突；已改为天蓝系 `(214,235,242)`，标签底色同步改深蓝 | 本轮修改 |
| 透明窗口文字 | 原用 `ClearTypeGridFit`，在逐像素 alpha 表面会产生彩色描边；已改为 `AntiAlias`（灰度抗锯齿） | 本轮修改 |
| 选中抬高 | 布局层按 `selected_elevation`(4dip) 位移卡片 `bounds` 并写入 `card.elevation`，约 4 逻辑像素这条已满足；渲染器原先另用 6/4 的厚度差，现已改为 `4dip + elevation`，与配置同源 | `SceneLayout.cpp:114,188`、`SceneRenderer.cpp` |
| 产品配置生成器 | **不幂等**：不写结尾换行，`TestProductIdentity` 每跑一次就把 `output/LeavesStoneProduct.nsh` 弄脏一次。已修为无 BOM + LF + 结尾换行，连跑两次哈希一致 | `scripts/generate-product-config.ps1` |

## 本轮已核实的事实

| 项目 | 结论 | 依据 |
| --- | --- | --- |
| 开发分支与提交 | `feature/leaves-stone-ime` @ `e6c18a32`，与交接包记录一致 | `git log -1` |
| 子模块 | librime `1c23358`、plum `cab9ed3` 及 librime 全部依赖已递归取得 | `git submodule status` |
| `TestLeavesStoneUI` 是否编得到 | 会。`weasel.sln` 含该项目且有 `Debug\|x64` / `Release\|x64` 的 `Build.0`，`build.bat` 走 `msbuild weasel.sln` | `weasel.sln`、`build.bat` |
| `TestLeavesStoneUI` 是否被执行 | 会，但此前存在假阳性：CI 的测试步骤用 `Get-ChildItem *Test*.exe` 遍历，找不到文件时循环为空、步骤照样通过 | 原 `leaves-stone-ci.yml` |
| 该假阳性 | 已修：无测试可执行文件即失败，且必须存在 `TestLeavesStoneUI.exe` | `.github/workflows/leaves-stone-ci.yml` |
| Release 安装包 | CI 原先只编 Debug x64，从不产出安装包；已新增 `release` job 编 Release Win32+x64 并跑打包 | 同上 |
| xmake 路径 | `xmake.lua` 的 debug 分支只含 `TestWeaselIPC`、`TestResponseParser`，且 `test/TestLeavesStoneUI/` 无 `xmake.lua`，故 xmake 构建不会含新测试。主链路是 msbuild，暂不改动以免弄坏 xmake 配置 | `xmake.lua`、`test/TestLeavesStoneUI/` |
| 主题分层素材 | 6 张已在 `resource/leaves-stone/`：`architectural-lines`、`building-ivory`、`building-terracotta`、`leaf-left`、`leaf-right`、`sky-blue` | 目录列举 |
| 原图 `visual-reference.png` | 不在仓库。`scripts/generate-theme-assets.ps1` 以它为输入，缺失时直接抛错，故素材无法重建，只能沿用已提交 PNG | `docs/assets/` 缺失、脚本第 13 行 |
| 打包/签名脚本 | 原先全部缺失（`package.ps1`、`sign-build.ps1`、`new-test-certificate.ps1`、`smoke-install.ps1`），本轮已补 | `scripts/` |

## 本机阻塞（未验证的直接原因）

1. **无编译器**：未安装 Visual Studio / MSBuild，`BOOST_ROOT` 未设置，无 NSIS。`scripts/verify-toolchain.ps1` 会判定 `Ready = false`，任何构建都无法复现，C++ 单测与安装包均跑不了。
   （CMake 其实未被任何构建步骤调用，librime 走 `get-rime.ps1` 下载的预编译包，属于检查脚本过严。）
2. ~~**无原图**~~ → 已解决（第五轮）。原图现位于 `docs/assets/visual-reference.png`，六张素材已证实由其派生。
   但“候选窗实际长什么样”仍需编译后运行才能看到，本文件的视觉行仍全部为 `未验证`。

## 验收行

| # | 验收项 | 状态 | 方法 | 证据 |
| --- | --- | --- | --- | --- |
| 1 | Windows 10 x64 安装 | 未验证 | `scripts/package.ps1` 产包 → `scripts/smoke-install.ps1` | |
| 2 | Windows 11 x64 安装 | 未验证 | 同上，换机器 | |
| 3 | 修复（覆盖安装） | 未验证 | `smoke-install.ps1` 的 repair 段 | |
| 4 | 升级 | 未验证 | 先装旧版再装新版，查词典保留 | |
| 5 | 卸载并保留用户词典 | 未验证 | `smoke-install.ps1` 的 uninstall 段 | |
| 6 | 记事本输入 | 未验证 | 手输简体全拼、中英切换 | |
| 7 | Word 输入 | 未验证 | 同上，含候选点击 | |
| 8 | Edge / Chrome 输入 | 未验证 | 同上 | |
| 9 | 微信输入 | 未验证 | 同上 | |
| 10 | Obsidian / Electron 输入 | 未验证 | 同上，重点看候选窗透明区与定位 | |
| 11 | 100% DPI | 未验证 | 单屏 | |
| 12 | 125% DPI | 未验证 | 单屏 | |
| 13 | 150% DPI | 未验证 | 单屏 | |
| 14 | 200% DPI | 未验证 | 单屏 | |
| 15 | 双屏、不同 DPI | 未验证 | 候选窗跨屏定位 | |
| 16 | 屏幕四边贴边 | 未验证 | 候选窗不越界、不反向 | |
| 17 | 32 位宿主 | 未验证 | 32 位记事本或 32 位 Electron 应用 | |
| 18 | 空候选 / 1 / 5 / 7 / 9 候选 | 未验证 | 设置里切候选数逐项试 | |
| 19 | 超长候选（超宽 + 超多） | 未验证 | 长词组、9 候选满页 | |
| 20 | 鼠标点击选词 | 未验证 | 命中区与视觉卡片对齐 | |
| 21 | 分页（方向键 / 翻页键 / 鼠标） | 未验证 | | |
| 22 | 连续输入不丢字 | 未验证 | 快速连打后核对上屏 | |
| 23 | 中英切换 | 未验证 | Shift 与语言栏两种方式 | |
| 24 | 视觉：独立凸起 3D 纸雕卡片 | 未验证 | 与用户认可的原图逐层比对；**不允许用生成图替代** | |
| 25 | 视觉：无统一可见大底板/外框 | 未验证 | 同上 | |
| 26 | 视觉：选中浅蓝并抬高约 4 逻辑像素 | 未验证 | 与 `leaves_stone_theme.yaml` 的 `selected_elevation: 4` 对应 | |
| 27 | 视觉：装饰不遮挡候选文字 | 未验证 | 逐字号检查 | |
| 28 | 中英文标点 | 未验证 | 两组标点各试一遍 | |
| 29 | 数字选词、空格选词 | 未验证 | | |
| 30 | 退格 / Esc 行为 | 未验证 | | |
| 31 | 本地用户词频生效 | 未验证 | 选生僻词多次后是否前移 | |
| 32 | 设置：5/7/9 候选 | 未验证 | 设置对话框 → 立即生效 | |
| 33 | 设置：字号、横竖排、动画、装饰 | 未验证 | 逐项 | |
| 34 | 设置：恢复默认 | 未验证 | | |
| 35 | 渲染资源重建后句柄稳定 | 未验证 | 反复重载主题，看 GDI/用户句柄数不涨 | |
| 36 | 不崩溃、不丢已提交文字 | 未验证 | 长时间使用 | |
| 37 | 不覆盖小狼毫既有身份与词典 | 未验证 | 同机共存小狼毫后检查两边数据 | |
| 38 | 日志不含用户完整提交文本 | 未验证 | 抓日志检索 | |
| 39 | 独立产品身份与 GUID | 未验证 | 查注册表 CLSID / 语言配置文件 | |
| 40 | 用户数据落在 `%APPDATA%\LeavesAndStone\Rime` | 未验证 | 安装后核对路径 | |
| 41 | GPL-3.0 与第三方许可随包 | 未验证 | 查安装目录与安装包 | |

## 结论文本口径

在 1–41 全部有证据之前，对外只能说“开发中测试包”，不能说“可安装测试版已就绪”。
第 24–27 行未通过前，视觉一律视为未验收。
