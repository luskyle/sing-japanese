# 假名小调

轻量离线日语桌面工具，只做两件事：学习假名、给日语文本注音并导出 A4 PDF。
使用 C++17、Qt 5 Quick/QML 和系统 MeCab/IPADIC；没有 Rust/Tauri、账号、网络服务或大模型。
界面采用暖白与鼠尾草绿配色，只保留假名学习、歌词注音两个页面。
预览与 PDF 共用振假名排版逻辑；保存 PDF 时使用系统文件对话框。

## 功能

- 平假名／片假名切换；清音、浊音、半浊音分类。
- 点击假名播放随程序打包的本地真人发音。
- 粘贴日语文本，使用本机 MeCab/IPADIC 标注振假名。
- 保留输入中的标点、空格及换行；未知汉字词保留原文并以点线标记。
- 将当前注音结果按 A4 页面导出为 PDF。原文修改后需重新注音才可导出。
- 不采集或保存学习记录；注音文本只保留在当前运行会话。

注音采用词典中的常规读音，人名、造词、歧义用法及歌手特殊唱法可能不准确。
每次最多处理 20,000 个 Unicode 字符。PDF 的振假名在词语上方显示，长文会自动换页。

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
  libqt5printsupport5 libmecab2 mecab-ipadic-utf8 fonts-noto-cjk \
  gstreamer1.0-plugins-good qml-module-qtquick2 qml-module-qtquick-window2 \
  qml-module-qtquick-controls2 qml-module-qtquick-layouts
```

## 从源码构建

```sh
sudo apt install build-essential cmake qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev \
  libqt5multimedia5-plugins libmecab-dev mecab-ipadic-utf8 \
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

Debian 包依赖 Qt 5、MeCab/IPADIC、Noto CJK 字体和 GStreamer Ogg 插件。IPADIC 声明保存在
[`public/licenses/IPADIC-NOTICE.txt`](public/licenses/IPADIC-NOTICE.txt)。

## 项目结构

```text
app/                 # C++ 状态与音频、MeCab 注音、振假名绘制与 PDF
app/qml/             # QML 双页界面与控件
tests/               # QML 加载与交互、注音、原文保留及 PDF 导出测试
public/audio/        # 离线假名录音，通过 Qt 资源嵌入程序
public/licenses/     # 词典许可声明
```

歌词注音页支持 `Ctrl+Enter` 注音、`Ctrl+S` 导出当前结果。
修改原文后必须重新注音再导出；未知汉字词以点线标记。

发音录音来自 Wikimedia Commons，录音者为 Hakatanoshio117117，按 Public Domain 发布。
来源和逐条校验信息见 [音频资源说明](public/RESOURCE-LICENSE.md)。
