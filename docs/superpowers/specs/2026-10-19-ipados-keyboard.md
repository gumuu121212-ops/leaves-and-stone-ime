# 小毛豆 iPad 触控输入法

## 已确认的目标

用户要求在 iPad 系统中安装、在其他 App 里实际打字的第三方键盘，主要使用屏幕触控键盘。沿用小毛豆、玳瑁眼镜和米白毛绒视觉。Windows 版、已有数据及素材原样保留。

## 首版行为

- iPadOS 15.0 及以上，原生宿主 App + Custom Keyboard Extension。
- 中文全拼、简体候选、英文切换、候选选择/翻页、退格长按重复、空格、符号、大小写及回车；提供系统键盘切换键和收起键。
- 复用现有 leaves_stone 方案和完整 Luna Pinyin 数据，保留 iPad/iPhone/iOS 名称及原文候选。不得用少数硬编码词冒充拼音引擎。
- 宽度至少覆盖 320、600、768、1024 pt，横竖屏及分屏改变宽度时保留组合状态。宽屏额外显示数字行，窄屏保留独立数字/符号面板。
- 小狗和候选条都在键盘扩展内部；预留狗头空间，不向其他 App 的文本区域绘图。小狗使用原趴睡/睁眼透明图层，切换不改变位置和造型；触控唤醒，闲置后闭眼。
- 中文组合串显示在键盘顶部，选词后才通过 textDocumentProxy 提交；不在宿主文本中插入再删除临时拼音。候选可以横向滚动，保留完整文字。
- 全部输入转换离线。RequestsOpenAccess=false，无输入上传、无输入内容日志，不依赖共享 App Group。词典在扩展自己的容器内保存。
- 切换文本输入对象、关闭键盘或离开 App 后，迟到的异步转换结果不能插入新输入对象。输入准备期间的按键按顺序排队。

## 实现边界

新增 platforms/ipados/，不修改 Windows TSF、服务、安装器或候选窗。共享的是现有 Rime 方案与原始狗素材，系统接入和渲染分别实现。UIKit 绘制键盘，Swift 管理交互，Objective-C++ 包装一个不依赖 Windows 的 C++ Rime 会话。

固定 librime-xcframework 1.17.0-pack.10.0.0 的静态 XCFramework，SHA-256 为 9e9923112ee9cee9bb53c25b41e51a2d7ea2ab3d91f34c7c99682224a23718ac。引擎仅静态链接进扩展和测试，不额外嵌入动态框架。携带上游及全部依赖许可。词库和 OpenCC 数据固定提交/摘要，在构建阶段生成预编译词典，避免首次打开键盘时现场编译。

## 验收

1. 真实引擎验证 nihao、完整长句、ipad、未知原文、分页、空格选词、退格和模式切换。
2. iOS 模拟器验证键盘控制器、异步输入顺序、文档切换保护、候选点击、不同宽度布局及按键取消。
3. Xcode 分别构建模拟器和 arm64 真机包；校验 App 内确有键盘 appex、框架、词典和许可。
4. 真机签名安装依赖用户自己的 Apple 开发签名/设备条件。未签名包不能称为可直接安装，模拟器验证不能称为真机验收。当前正在等待用户回复是否有 Mac；可先用 GitHub macOS 构建完成工程验证。

## 依据

- https://developer.apple.com/documentation/uikit/creating-a-custom-keyboard
- https://developer.apple.com/library/archive/documentation/General/Conceptual/ExtensibilityPG/CustomKeyboard.html
- https://github.com/ghostflyby/librime-xcframework/tree/1.17.0-pack.10.0.0

