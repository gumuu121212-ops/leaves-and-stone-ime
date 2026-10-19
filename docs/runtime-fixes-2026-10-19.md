# 小毛豆输入法运行问题接续记录

基线为 `b9a9f1b`，对应 `codex-handoff-2026-10-19.zip`。本批保持既有狗素材、候选条形状、工具条大小与安装器文案。

## 本批修改

- 内嵌预编辑时，优先查询当前选区活动端的真实坐标，再尝试组合末端。新组合尚未获得精准坐标时可使用起点回退；已获得精准坐标后，临时没有布局就保留上次位置，避免跳回起点。非内嵌模式保留原来的起点优先行为，包括 CUAS 零高度检测。不按字数估算位置。
- 内嵌文字完成设置选区后再请求一次位置更新；释放 GetSelection 返回的范围，避免原有引用泄漏。
- 补齐原已声明但未订阅的 ITfActiveLanguageProfileNotifySink，并在 QueryInterface 中公开它；停用输入法、失去线程焦点时通知服务端隐藏工具条，恢复焦点时通知显示。
- 服务端按当前获得焦点的会话显示工具条；后台会话释放不会重新显示，后台迟到的失焦不会隐藏当前会话。提交或清空组合串不再冒充获得焦点，避免“失焦 → 清空组合串 → 工具条又出现”。
- 初始工具条定位不再带 SWP_SHOWWINDOW；隐藏时停止指针轮询并复位睁眼状态。
- GetIcon 记录实际加载来源、模块、状态、尺寸和错误码。无效方案图标回退到内置资源；返回可由 TSF 调用者销毁的独立图标，不返回 LR_SHARED。
- 产品显示名的维护源 config/product.json 改为“小毛豆输入法”，并同步身份测试，防止 CI 生成步骤覆盖回 Leaves & Stone。

## 检查情况

本地七项 PowerShell 测试通过：Toolchain、BuildLocal、ConfigContract、SceneContract、RendererContract、SettingsContract、ProductIdentity。身份测试先验证了旧配置会失败，修正维护源后通过。

新增 C++ 回归测试已接入 TestLeavesStoneUI：

- CaretPosition：真实光标前进/回退、负坐标、换行位置、选区不可用、末端不可用、非内嵌模式与全部失败时的回退行为。
- ToolbarVisibility：真实窗口创建后必须隐藏，显隐切换后保持正确状态，隐藏后的指针定时器不能重新显示。

`837d5b4` 已通过 [GitHub Actions](https://github.com/gumuu121212-ops/leaves-and-stone-ime/actions/runs/39142398921) 的 Debug 原生测试与 Win32/x64 Release 构建。本机缺少 MSBuild、Windows SDK 等构建依赖，后续修改的原生测试也必须由对应 CI 执行；安装后的实际行为仍需针对具体构建验收。

现有 workflow 的 push 触发仅监听 main；当前 feature 分支对应已有 PR #1，其 pull_request 同步事件可触发构建，不必修改 workflow。

## 安装后的运行验收

### 候选条离输入行过远的后续修正

用户安装 `837d5b4` 后反馈候选条偏低。场景原先把包含小狗的整个窗口顶边放在光标下方，导致候选条上沿额外下移 `102 × dpi_scale` 像素。

现在以候选条上沿对齐 `anchor.bottom + 4 × dpi_scale`，小狗向上探出；空间不足时整个场景翻到输入行上方，保留原有屏幕边缘约束。这里的 anchor 仍包含通用 MoveTo 路径原有的 6 像素偏移。狗素材、搭边基线和条内布局不变。

SceneLayout 回归测试覆盖内嵌/非内嵌预编辑、100%/125%/150%/200% 缩放、底部空间足够时不提前翻转、空间不足时向上避让、顶部边缘与负坐标副屏。坐标日志增加实际 `bar` 位置与 `above` 状态，便于安装后核对。

### 输入过程反复跳动的后续修正

已按 SHA-256 确认运行中的 WeaselServer 与 `88670f0` 构建一致。运行日志显示同一输入行中，选区与组合末端临时返回 `TS_E_NOLAYOUT`（`0x80040206`），组合起点仍返回有效坐标 `1698,1056,1698,1092`；约 16–47 毫秒后选区重新返回有效末端位置，如 `1792,1056,1792,1092`。旧逻辑每次接受起点回退，造成候选条先左跳再右跳。[微软文档](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfcontextview-gettextext)说明该错误表示文本尚未渲染或布局尚未计算完成。

`CaretPositionState` 按每次组合保存是否已获得精准坐标；一旦选区或末端成功，临时失去布局时不再接受起点回退，也不发送新的位置。现有 `OnLayoutChange` 和后续位置查询取得有效坐标后立即跟随。组合开始、结束时重置状态，使下一次组合和其他输入位置仍能正常初始化；非内嵌模式不受限制。

回归测试先在[旧逻辑构建](https://github.com/gumuu121212-ops/leaves-and-stone-ime/actions/runs/39181772743)中实际失败，Win32/x64 均复现了“精准光标退回组合起点”。用例同时覆盖多次暂时缺失布局、正常退格、真实换行、组合状态重置及组合末端成功后的稳定性。修复没有增加等待定时器或修改动画与皮肤距离。

### 安装后检查

1. 在记事本、微信和 VS Code 输入逐渐变长的拼音，再按左右键移动拼音内的光标、退格、输入到换行位置。候选窗应跟随真实光标；如宿主没有提供有效末端布局，允许保留起点回退。
2. 用 Win+Space 在小毛豆、微软拼音、ENG 之间反复切换。离开小毛豆后工具条应隐藏；返回后显示。保留多个应用窗口，并检查切到后台的旧会话不会重新唤醒工具条。
3. 输入拼音但不上屏，直接切换输入法，确认清空组合串不会让工具条重新出现。
4. 重启服务后尚未选择小毛豆时，工具条不能因为初始化定位而出现。
5. 当前版本使用每进程最多 128 条的 `%TEMP%\leaves-stone-tsf.log`。只记录坐标与资源元数据，不记录输入的文本内容。

## 任务栏“中”的证据要求

这一项还不能报告已修复，也不能仅以旧图标仍在显示就断言 Windows 不读取输入法图标。

安装新构建并重新打开测试应用后，切换中/英，核对日志的 `icon-v2`：

- `module`：当前实际提供图标的 TSF DLL 路径，区分宿主加载的旧 DLL 和新 DLL。
- `source=resource` 与 `id`：是否使用内置中/英图标。
- `source=file` 与 `configured`：是否被输入方案指定的文件覆盖。
- `configured_error` / `load_error`：配置图标失败以及最终加载的错误码。
- `size` / `ascii`：系统实际请求的尺寸与模式。

没有 icon-v2 记录时，要先排除旧模块仍驻留、尚未触发图标请求以及当前图标来自其他提供者；不能直接据此判断系统完全不支持替换。即便成功提供图标，彩色显示仍需实际验证，微软 IME 规范要求黑白图标。

依据：[IME requirements](https://learn.microsoft.com/en-us/windows/apps/develop/input/input-method-editor-requirements)、[GetIcon 所有权](https://learn.microsoft.com/en-us/windows/win32/api/ctfutb/nf-ctfutb-itflangbaritembutton-geticon)、[语言配置切换通知](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfactivelanguageprofilenotifysink)。
