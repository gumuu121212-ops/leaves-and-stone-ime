# Leaves & Stone 输入法

Windows 上的独立中文输入法，基于 [Weasel 0.17.4](https://github.com/rime/weasel) 与
[librime](https://github.com/rime/librime)，沿用 TSF 输入法框架，不另造中文输入引擎。

## 当前状态（请先读这一节）

**这是开发中的测试包，还不是可安装测试版。** 仓库里没有任何一行验收结论是“已通过”的，
详细逐项状态见 [docs/qa-matrix.md](docs/qa-matrix.md)。

- 候选窗视觉**尚未验收**。交接文档记载用户明确否定过一版生成图，要求回到原建筑植物插画
  （暖白、砖红、橄榄绿、浅天蓝、黑色建筑线稿；每个候选是独立凸起的 3D 纸雕卡片，
  不能包在统一可见的大底板里）。没有原图就无法确认现状是否达标。
- Release 安装包、签名、跨应用输入、多 DPI、32 位宿主均**未经实际验证**。
- 本仓库不保存私钥。测试包的签名方式见 [docs/testing-certificate.md](docs/testing-certificate.md)。

## 适用范围

- Windows 10 / 11 x64；兼容 32 位宿主进程。
- 首版方案：简体全拼、英文直输、中英文标点、数字与空格选词、鼠标选词、方向键与翻页、
  退格 / Esc、本地用户词频与词典。
- 设置只有：候选数 5 / 7 / 9、字号、横排竖排、动画、装饰、恢复默认。

**不做**：账号、云词库、云同步、皮肤商店、主题导入、手机 / macOS / Linux、
双拼 / 五笔 / 手写 / 语音、自动更新、广告、遥测。

## 产品身份与数据位置

| 项目 | 位置 |
| --- | --- |
| 注册表根 | `HKEY_LOCAL_MACHINE\Software\LeavesAndStone\IME` |
| 用户数据（词典、配置） | `%APPDATA%\LeavesAndStone\Rime` |
| 卸载入口 | 设置 → 应用，或安装目录下的 `uninstall.exe` |

产品身份与 GUID 定义在 [include/LeavesStoneProduct.h](include/LeavesStoneProduct.h)，
由 [config/product.json](config/product.json) 经 `scripts/generate-product-config.ps1` 生成。
**不要**把小狼毫的注册表项或用户词典当成本产品的数据，两者互不覆盖。

## 构建

需要：Visual Studio 2022（Desktop development with C++）、Windows 10/11 SDK 与 SignTool、
CMake、Boost 1.84.0（`BOOST_ROOT` 指向解压目录）、NSIS 3、Git、clang-format 18.1.6+。

```powershell
# 先确认工具链
powershell -ExecutionPolicy Bypass -File scripts/verify-toolchain.ps1

# Debug x64（开发用）
powershell -ExecutionPolicy Bypass -File scripts/build-local.ps1 -Configuration Debug -Platform x64

# Release Win32 + x64 与安装包
powershell -ExecutionPolicy Bypass -File scripts/build-local.ps1 -Configuration Release -Platform All
```

安装包输出在 `output/archives/`。完整说明见 [docs/development.md](docs/development.md)。

> 本机策略是 AllSigned 时，脚本需按上面这样带 `-ExecutionPolicy Bypass` 调用。

## 测试

```powershell
# 静态合约测试（不需要编译器）
Get-ChildItem test\Test*.ps1 | ForEach-Object { powershell -ExecutionPolicy Bypass -File $_.FullName }

# 编译后的 C++ 单元测试
output\TestLeavesStoneUI.exe
```

注意：`test\Test*Contract.ps1` 只检查文件存在与关键字符串打点，属于防误删的浅层护栏，
**不能**证明渲染或行为正确；真正的行为断言在 `test/TestLeavesStoneUI/` 的 C++ 测试里。

## 打包与签名

```powershell
# 出包并生成 SHA256SUMS.txt 与 manifest.json
powershell -ExecutionPolicy Bypass -File scripts/package.ps1

# 生成一次性测试证书（私钥强制落在仓库外）
powershell -ExecutionPolicy Bypass -File scripts/new-test-certificate.ps1

# 带签名出包
powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -Sign -Thumbprint <thumbprint>

# 安装 / 维修 / 卸载冒烟（需管理员终端）
powershell -ExecutionPolicy Bypass -File scripts/smoke-install.ps1
```

打包脚本会先扫描仓库里的 `.pfx` / `.p12` / `.key` / `.pem`，发现私钥即拒绝出包。

## 文档

- [docs/qa-matrix.md](docs/qa-matrix.md)：41 项验收矩阵与当前证据（唯一可信的状态来源）
- [docs/development.md](docs/development.md)：工具链与构建
- [docs/testing-certificate.md](docs/testing-certificate.md)：测试证书的信任与撤销
- [docs/superpowers/](docs/superpowers/)：原始设计与实施计划（历史需求资料，不是完成报告）

## 许可与来源

本项目是 Weasel 的衍生作品，遵循 **GPL-3.0**，见 [LICENSE.txt](LICENSE.txt)。
librime、plum 及其依赖各自遵循其原始许可；主题素材来源见
[resource/leaves-stone/README.md](resource/leaves-stone/README.md) 与
[output/data/LEAVES_STONE_DATA_LICENSES.md](output/data/LEAVES_STONE_DATA_LICENSES.md)。
上游项目与版本对应关系见 [UPSTREAM.md](UPSTREAM.md) 与 [CHANGELOG.md](CHANGELOG.md)。
