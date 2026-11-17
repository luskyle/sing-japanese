# 假名小调

轻量离线日语桌面工具，只做两件事：学习假名、给日语文本注音并导出 DOCX / A4 PDF。
使用 C++17、Qt 5 Quick/QML、Minizip 和系统 MeCab/IPADIC；没有 Rust/Tauri、账号、网络服务或大模型。
界面采用暖白与鼠尾草绿配色，只保留假名学习、歌词注音两个页面。
预览与 PDF 共用振假名排版逻辑；保存 PDF 时使用系统文件对话框。

## 功能

- 平假名／片假名切换；清音、浊音、半浊音分类。
- 假名表根据窗口大小分栏，全部 71 个假名同屏展示，无需滚动；选中假名的发音信息显示在底部。
- 点击假名播放随程序打包的本地真人发音。
- 粘贴日语文本，使用本机 MeCab/IPADIC 标注振假名。
- 界面支持系统高 DPI 缩放，控件随窗口放大；小窗口自动换行、纵向排列并可滚动。
- 默认窗口为 1360 × 900 逻辑像素（小屏幕自动适配）。
- 窗口最小尺寸为 1120 × 800 逻辑像素，不能继续缩小；屏幕可用区域不足时，以下限不超过可用区域为准。系统高 DPI 缩放会自动换算为实际像素。
- 原文按 A4 页宽连续编辑；注音稿按 A4 纸张与 18 毫米页边距分页预览。正文使用 12 磅，注音使用 7.5 磅。
- 原文与注音稿占满可用高度，注音、预览、复制和导出按钮固定在页面底部；长文仅在纸张区域内滚动。
- 默认按面板宽度缩放纸张，可用「放大到 100% / 适合宽度」切换；页面缩放只影响屏幕显示，不影响导出字号、换行或分页。
- 注音稿按读音宽度自动留出原文间距，假名居中显示在对应汉字上方；行高随字体度量计算，避免上下行重叠。
- 「汉字加注罗马音」在汉字读音上增加罗马音层；「全部内容加注罗马音」额外给汉字、平假名、片假名等内容加注罗马音。
  两项独立勾选、即时生效，均不替代原有假名；同时勾选时保留两个罗马音层。
- 保留输入中的标点、空格及换行；未知汉字词保留原文并以点线标记。
- 支持预览 DOCX、预览 PDF、导出 DOCX / PDF，以及复制全部注音稿到 Word。
- 不采集或保存学习记录。文档预览使用临时文件，正常退出时清理；导出的文件和剪贴板内容由用户管理。

注音采用词典中的常规读音，人名、造词、歧义用法及歌手特殊唱法可能不准确。
每次最多处理 20,000 个 Unicode 字符。预览与 PDF 都会利用原文中的假名定位读音，
例如「帰る」只在「帰」上方标注「かえ」。连续汉字词或无法唯一拆分的读音保留整词注音，
不猜测逐字读音。罗马音采用常见 Hepburn 拼写，长音以重复元音表示（例如「スーパー」为 `suupaa`）；
按词典识别的助词「は／へ／を」写作 `wa / e / o`，未知汉字不猜读音，在全文罗马音层显示 `?`。

### 文档预览与复制到 Word

1. 完成注音，勾选需要的罗马音层。改变选项无需重新注音；修改原文后必须重新注音才能预览、复制或导出。
2. 点击「预览 DOCX」或「预览 PDF」，程序生成当前结果的临时文档，并交给系统关联应用打开。
   DOCX 需要安装 Word、WPS 或 LibreOffice；PDF 需要 PDF 阅读器。预览文档是生成时的快照，改变内容后应再次预览。
3. 点击「复制到 Word」，在目标文档中以**保留源格式**粘贴；剪贴板同时提供包含全部页面的 HTML 和 RTF 图片内容。
   不要使用「仅保留文本」。不同编辑器对剪贴板格式支持不同；如不能粘贴图片，请直接使用「导出 DOCX」。

**DOCX 和复制内容采用 3 倍分辨率的图片式排版，文字不可单独编辑。** 它们与注音稿共用 A4 绘制器，
保留相同的换行、分页、所有注音层、字号比例与对齐；不依赖目标电脑的字体或 Word 对振假名的支持。
PDF 也使用同样的 A4 排版。极长且不可拆分的词组会等比缩小以免裁切。
导出文件可在程序退出后继续使用；预览临时文件不可作为永久保存。

## 安装与运行（Ubuntu / Debian）

下载 Debian 安装包后，在安装包所在目录执行：

```sh
sudo apt install ./sing-japanese-0.3.0-Linux.deb
sing-japanese
```

使用 `apt install ./包名.deb` 安装会一并安装运行依赖；不要只从包中提取可执行文件运行。
安装包本身约 2 MB，不包含系统 Qt 库和 IPADIC 词典，首次安装还需下载这些依赖。

如果直接运行自行编译的程序时出现 `libQt5Multimedia.so.5` 或 `libmecab.so.2` 缺失：

```sh
sudo apt install libqt5widgets5 libqt5multimedia5 libqt5multimedia5-plugins \
  libqt5printsupport5 libmecab2 libminizip1 mecab-ipadic-utf8 fonts-noto-cjk \
  gstreamer1.0-plugins-good qml-module-qtquick2 qml-module-qtquick-window2 \
  qml-module-qtquick-controls2 qml-module-qtquick-layouts
```

## 从源码构建

```sh
sudo apt install build-essential cmake qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev \
  libqt5multimedia5-plugins libmecab-dev libminizip-dev mecab-ipadic-utf8 \
  fonts-noto-cjk gstreamer1.0-plugins-good qml-module-qtquick2 \
  qml-module-qtquick-window2 qml-module-qtquick-controls2 qml-module-qtquick-layouts
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sing-japanese
```

如同时安装了 Conda 的 Qt，构建时可使用
`-DQt5_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt5 -DCMAKE_PREFIX_PATH=/usr`
选择系统 Qt，避免混用不同版本的库与 QML 插件。

运行 MeCab 注音需要系统 IPADIC。CMake 会将它作为系统依赖，而非复制进应用。
如果系统找不到词典，请确认 `mecab-ipadic-utf8` 已安装且 `/etc/mecabrc` 指向该词典。
Ogg 发音通过 Qt Multimedia / GStreamer 播放。首次播放时录音会从应用资源复制到用户缓存目录。

运行测试并生成 Debian 安装包：

```sh
ctest --test-dir build --output-on-failure
cd build
cpack -G DEB
```

Debian 包依赖 Qt 5、Minizip、MeCab/IPADIC、Noto CJK 字体和 GStreamer Ogg 插件。IPADIC 声明保存在
[`public/licenses/IPADIC-NOTICE.txt`](public/licenses/IPADIC-NOTICE.txt)。

## 项目结构

```text
app/                 # C++ 状态与音频、MeCab 注音、罗马音、共享绘制与 DOCX/PDF
app/qml/             # QML 双页界面与控件
tests/               # QML 加载与交互、注音、原文保留及 PDF 导出测试
public/audio/        # 离线假名录音，通过 Qt 资源嵌入程序
public/licenses/     # 词典许可声明
```

歌词注音页支持 `Ctrl+Enter` 注音、`Ctrl+S` 导出当前结果。
修改原文后必须重新注音再导出；未知汉字词以点线标记。

发音录音来自 Wikimedia Commons，录音者为 Hakatanoshio117117，按 Public Domain 发布。
来源和逐条校验信息见 [音频资源说明](public/RESOURCE-LICENSE.md)。
