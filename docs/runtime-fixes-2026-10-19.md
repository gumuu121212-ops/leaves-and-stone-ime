# 小毛豆输入法运行问题接续记录

基线为 `b9a9f1b`，对应 `codex-handoff-2026-10-19.zip`。本批保持既有狗素材、候选条形状、工具条大小与安装器文案。

## 本批修改

- 内嵌预编辑时，优先查询当前选区活动端的真实坐标；失败时依次查询组合末端和组合起点。非内嵌模式保留原来的起点优先行为，包括 CUAS 零高度检测。无有效布局时保留上次位置，不按字数估算。
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

本机缺少 MSBuild、Windows SDK 等构建依赖，尚未执行完整 C++ 编译或这两项原生测试，也未安装新版本。必须在现有 PR 的 GitHub Actions 完成 Debug 测试、Win32/x64 Release 构建后，才能进行运行验收。

现有 workflow 的 push 触发仅监听 main；当前 feature 分支对应已有 PR #1，其 pull_request 同步事件可触发构建，不必修改 workflow。

## 安装后的运行验收

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
