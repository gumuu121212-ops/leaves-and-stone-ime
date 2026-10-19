# 小毛豆 iPad 触控键盘

这是原生 iPadOS 自定义键盘扩展，配有用于启用键盘、试打和查看许可的主 App。最低 iPadOS 15.0，使用原来的小毛豆透明素材与平面米白配色。候选条和按键使用纯色及细分隔线，保留米色选中块。中文全拼由离线 librime 和完整 Luna 词库处理，也可切换英文、数字和符号。

## 使用

安装经过 Apple 开发者签名的 App 后，在「设置 → 通用 → 键盘 → 键盘 → 添加新键盘」中添加「小毛豆」。回到输入框，长按地球键选择小毛豆。无需开启「允许完全访问」。密码框、部分特殊输入框以及禁止第三方键盘的 App 会使用系统键盘。

中文拼音显示在键盘上方，点候选或按空格上屏；候选条可横向滑动，右侧箭头翻页。英文模式直接输入；按住退格连续删除，松手停止。数字和符号页使用左下角切换；右下角收起键盘。小狗闲置 8 秒闭眼，触摸键盘时睁眼。横竖屏和窄窗口共用当前拼音状态。

## Mac 上构建与安装

需要安装 Xcode 的 Mac；Windows 不能直接用 Xcode 给 iPad 安装此键盘。本项目的 GitHub macOS 构建可先生成未签名的设备产物。

1. 在仓库根目录运行 `bash platforms/ipados/tools/build-native.sh`，准备固定版本依赖和预编译词库；这一步需要联网，键盘使用时不需要联网。
2. 运行 `swift platforms/ipados/tools/make-icon.swift "$PWD/platforms/ipados"` 和 `python3 platforms/ipados/tools/generate_project.py`。
3. 用 Xcode 打开 `platforms/ipados/Xiaomaodou.xcodeproj`，选择 Xiaomaodou scheme。
4. 在 App 和 XiaomaodouKeyboard 两个 target 的 Signing & Capabilities 里选择自己的开发团队。把 App 的 Bundle Identifier 改为自己可签名的标识，例如 `your.domain.xiaomaodou`，扩展使用同一前缀加 `.keyboard`。
5. 连接自己的 iPad，按 Xcode 提示开启开发者模式并运行。成功安装后按上文启用键盘。

`tools/build.sh` 会运行模拟器测试、生成界面附件，再 archive arm64 设备 App，产出 `dist/Xiaomaodou-iPad-UNSIGNED.ipa` 和校验清单。**UNSIGNED.ipa 未签名，不能直接点开安装，也不代表已在真机验证。** App Store / TestFlight 分发需要相应 Apple 开发者账号、签名和上架流程；本包没有替你申请或完成这些步骤。

## 数据与许可

键盘未请求完全访问，不发送输入内容，不使用共享 App Group，用户词典保存在扩展自己的 Application Support 目录。切换到其他输入框时会丢弃旧输入框仍在排队的上屏结果。原始拼音在候选区内呈现，只把最终上屏文本交给宿主 App。

本项目代码 GPL-3.0。原有小毛豆照片及衍生素材由用户确认具有相应权利，并授权按 GPL-3.0 再分发；iPad 版本直接复用原透明素材，没有重新生成狗的造型。librime BSD-3-Clause，Luna / essay 词库及其许可、OpenCC 和静态库所含依赖的声明均保留于 App 中的 `Notices` 目录。具体固定版本及摘要见 `dependencies.json`，完整许可在主 App「开源许可与素材说明」中查看。

首次输入直接读取构建时生成的二进制词库；扩展不编译词库、不启动 Rime maintenance。使用静态 XCFramework，不嵌入未标记为扩展安全的动态 librime。触控键盘与 Windows 的桌面悬浮工具栏分别实现，iPad 版的绘制和触摸均限制在系统键盘区域。
