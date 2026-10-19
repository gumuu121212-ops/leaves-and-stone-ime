# 小毛豆输入法安装器：精确布局与素材

本包是新的、独立的 setup-ui 交付，包含「选择安装位置、正在安装、安装完成」全部三页。名称统一为「小毛豆输入法」。旧素材包没有修改。

## 入口

- **layout.json**：唯一布局依据。每页都包含完整 elements 数组，公共元素已经展开，无需合并其他文件或推算父坐标。
- **index.html**：双击即可离线打开的 HTML/CSS 参考；窗口下方有三页查看按钮，查看按钮不属于安装器设计。
- **styles.css / app.js**：直接读取布局，字体、控件位置、颜色和状态来自 JSON。
- **layout.js**：layout.json 的逐值相同副本，供 file:// 打开时使用，避免浏览器本地 fetch 限制。修改 JSON 后请同步重新生成该文件，不要分别编辑两套值。
- **previews/directory.png、installing.png、finished.png**：同一 HTML 在 100% 缩放、deviceScaleFactor=1 下截取的 1280×900 基准图。
- **assets/**：全部独立透明 PNG；不从预览图中抠取。

## 单位、DPI 与旧包的区别

**本次新交付统一以 1280×900 为设计画布，参考 DPI=96。1 个 JSON 单位 = 1 个参考像素 = 浏览器 100% 缩放时的 1 CSS px。96 DPI 下也等于 1 个物理像素。**

不要继续套用旧 doggy-installer.zip 中的 assetScale=2、640×450 dip 解释；不要把本包坐标除以 2。这是本次交付明确统一后的坐标体系。

原生程序若进行 DPI 缩放：

| 系统 DPI | 坐标与字号乘数 | 完整窗口物理像素 |
|---|---|---|
| 96 | 1 | 1280×900 |
| 120 | 1.25 | 1600×1125 |
| 144 | 1.5 | 1920×1350 |

使用浮点数统一缩放坐标、边框、圆角、文字基线、字体像素字号和图片目标矩形，最后由同一个绘制系统处理像素落点。不要分别按不同公式缩放文字与图片。较小屏幕的适应缩放属于宿主功能，不在本稿中另设第二套坐标。

参考窗口为自绘标题栏；原生标题栏不能再叠加在画布上。窗口外的 DWM 阴影不计入 1280×900；基准图没有窗口外阴影。

三张基准 PNG 的圆角外部为透明像素，可直接叠到宿主截图上核对窗口区域。

## layout.json 结构

根级记录 canvas、referenceDpi、coordinateUnit、fontSizeUnit、product、font、colors、assets、referenceData、referenceRenderer 与 pages。

三页键名：

- pages.directory：选择安装位置。
- pages.installing：正在安装，固定展示 37% 参考状态。
- pages.finished：安装完成。

每页 elements 的每一项都包含唯一 id、type、rect。所有 rect 都是**画布内绝对坐标 [x,y,width,height]**，不是相对父元素的位置。顺序就是绘制顺序；背景在先，控件和文字在后。

文字项完整写出 text、font、size、weight、color、lineHeight、align、verticalAlign、letterSpacing 和 rect；另附从参考 HTML 实际读取的 baselineY 与字体框 ascent/descent。需要按基线绘制时，使用 baselineY 作为未缩放的绝对 y 坐标。文本 rect 负责逻辑排布和裁切，不是文字实际墨迹范围。

按钮项包含 enabled、visible、action、states.normal / hover / pressed / disabled / focus。禁用态不应响应点击，visible=false 则不绘制也不进入焦点顺序。按下状态的边框和颜色见各元素完整定义。标题栏图标按钮提供绝对折线路径，完成页成功标志也提供圆与路径坐标。

field 是实际输入控件规格，包含 padding=[top,right,bottom,left]、边框和焦点状态。参考 HTML 的焦点边框采用原 2 px 边框 + 内侧 2 px 阴影表现为 4 px，避免焦点切换改变文字位置。

progressBar 包含 rect、background、fill、radius、value、fillRule。37% 时按 812×0.37 计算宽度，referenceFillRect 记录 Chromium 的实际子像素矩形。真实进度由宿主传入，不由 UI 定时伪造。

binding 只表示需要从运行时数据读取；text / value 保存参考截图所使用的示例值。进度、磁盘空间与路径必须由真实安装器更新，不能将示例值当成当前环境数据。

## 三页交互与按钮状态

| 页面 | 上一步 | 中间主按钮 | 取消 | 标题栏关闭 |
|---|---|---|---|---|
| 选择安装位置 | 显示、禁用 | 「安装」，初始有效路径下启用 | 显示、启用 | 取消安装向导 |
| 正在安装 | 显示、禁用 | 「安装中…」，禁用 | 显示、启用；请求取消并等待宿主确认 | 与取消相同，先确认 |
| 安装完成 | 显示、禁用 | 「完成」，启用 | 隐藏、禁用 | 完成并关闭向导 |

第一页真实程序应检查路径合法性、写入条件和空间；不符合时禁用安装。HTML 只验证非空，不能替代原生文件系统检查。目录选择由宿主系统选择器实现。

第二页进入不能取消的阶段时，宿主同时禁用取消与关闭；最小化仍可用。完成页只能在真实安装成功后进入，不能因进度到 100% 就无条件宣告成功。错误与取消后的文案由宿主另行处理，本包没有为其伪造完成页。

## 颜色与控件状态

| 项目 | 背景 / 填充 | 文字 / 前景 |
|---|---|---|
| 窗口 | #FBF8F1 | 正文 #554638；标题 #4C3B2C；次文字 #9A856E |
| 标题栏 | #F2EBDF | #514438；底线 #E3D8C7 |
| 左侧卡片 | #F0E5D5 → #F6EEE2，纵向渐变 | 以元素各自 color 为准 |
| 信息卡片 | #F1E8DB | 数值 #66503A；标签 #A28C73 |
| 主按钮：正常 | #D5BDA7 | #4A3423 |
| 主按钮：悬停 | #CBB095 | #4A3423 |
| 主按钮：按下 | #BFA084 | #4A3423 |
| 主按钮：禁用 | #E7DCCD | #B5A591 |
| 次按钮：正常 | #F9F5ED | #7C644C |
| 次按钮：悬停 | #EEE2D2 | #6F563F |
| 次按钮：按下 | #E1CEB7 | #4A3423 |
| 次按钮：禁用 | #F2EDE5 | #B5A591 |
| 进度条 | 轨道 #E9DFD1；填充 #BA9B79 | 高 22 px、圆角 11 px |

常规按钮边框：正常 #D8C6B0、悬停 #C6AA89、按下 #B69672、禁用 #E8E0D4，宽度均为 2 px；键盘焦点轮廓 #A6815B，2 px、向外偏移 3 px。按钮圆角 22 px。

输入框底色 #FFFDFA，普通边框 #DCCDBB、2 px，焦点边框 #B79572、总视觉宽 4 px，圆角 20 px。标题栏按钮的各状态颜色与路径均另列在各元素 states 和 paths 中，不使用系统默认按钮背景替代。

## HTML/CSS 使用与宿主接口

双击 index.html 可查看。也可使用 `index.html?page=directory`、`?page=installing` 或 `?page=finished` 直接打开某页。浏览器缩放保持 100%；基准截图只截 #setupWindow，不包括外围的设计稿查看按钮和说明。

在宿主中可以调用：

```js
SETUP_UI.setPage('installing');
SETUP_UI.setData({
  directory: 'D:\\04 Applications\\',
  progress: 0.5,
  currentStep: '正在配置输入环境…',
  canCancel: true
});
SETUP_UI.setData({ canCancel: false }); // 同时禁用取消和关闭
// 只有真实安装成功后：
SETUP_UI.setPage('finished');
```

setData 会从 progress 自动生成百分比文字；也可同时传 progressPercent 自定义文字。运行时字段包括 title、version、directory、requiredSpace、freeSpace、progress、progressPercent、currentStep、canCancel。

宿主可提供：

```js
window.SETUP_HOST = {
  onAction(action, context) {
    // action: browse / start-install / cancel / minimize / close / finish
    // context: 当前页面、目录和参考数据
    // 在这里连接真实目录选择、窗口操作与安装逻辑
  }
};
```

也会分发 setup-action 自定义事件。没有宿主时，HTML 仅演示页面切换、按钮悬停 / 按下 / 禁用、路径编辑和取消确认；不会安装文件、修改注册表、读取磁盘空间或关闭操作系统窗口。标题栏拖动区域在 JSON 和 DOM 中标出，由原生宿主执行窗口拖动。

## 字体

所有安装器文字统一 **Microsoft YaHei UI**，字重仅 400 与 700。所有 size 都是**像素字号 px**，不是 pt。lineHeight 和 baselineY 同样是参考像素。

本机基准使用系统实际的 MicrosoftYaHeiUI 与 MicrosoftYaHeiUI-Bold 字体，已通过浏览器平台字体信息核对。没有使用非系统字体，不打包微软字体文件。字体版本或渲染器不同时，抗锯齿边缘可能不同；字体、DPI、浏览器版本和渲染条件已记录在 referenceRenderer。原生 DirectWrite/GDI 实现应对照三张 PNG 核对文字基线与边缘，而不是承诺不同渲染引擎自动逐像素一致。

## 素材

| 文件 | 尺寸 | 内容 |
|---|---|---|
| assets/titlebar-dog.png | 64×64 | 独立狗头，透明 |
| assets/mascot.png | 600×340 | 小毛豆趴姿，透明 |
| assets/paw.png | 96×96 | 爪印，透明 |
| assets/cushion.png | 544×144 | 额外的独立绒面垫，用于还原左侧插画中的底垫 |

四张均为 8 位每通道、color type 6、RGBA 真透明 PNG，带透明与部分透明边缘。不烘焙任何页面文字。mascot.png 自带透明留白，按 JSON 矩形绘制，不再逐图自动裁边。

## 来源与许可

小毛豆、玳瑁眼镜、绒面垫与爪印沿用本聊天已生成的独立素材，再做精确尺寸整理，没有从预览截图裁图或抠图。原始照片由用户提供；用户已确认有相应权利，同意衍生素材按 GPL-3.0 再分发。

本包采用 GPL-3.0-only，全文见 LICENSE.txt。本包是安装器 UI 规格、素材与可运行视觉参考，不包含实际安装业务实现。

