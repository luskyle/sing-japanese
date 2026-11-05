# 假名小调

轻量离线日语桌面工具，只做两件事：学习假名、给日语文本注音并导出 A4 PDF。
使用 C++17、Qt 5 Widgets 和系统 MeCab/IPADIC；没有 Rust/Tauri、账号、网络服务或大模型。

## 功能

- 平假名／片假名切换；清音、浊音、半浊音分类。
- 点击假名播放随程序打包的本地真人发音。
- 粘贴日语文本，使用本机 MeCab/IPADIC 标注振假名。
- 保留输入中的标点、空格及换行；未知汉字词保留原文并以点线标记。
- 将当前注音结果按 A4 页面导出为 PDF。原文修改后需重新注音才可导出。
- 不采集或保存学习记录；注音文本只保留在当前运行会话。

注音采用词典中的常规读音，人名、造词、歧义用法及歌手特殊唱法可能不准确。
每次最多处理 20,000 个 Unicode 字符。PDF 的振假名在词语上方显示，长文会自动换页。

## 构建与运行（Ubuntu / Debian）

```sh
sudo apt install build-essential cmake qtbase5-dev qtmultimedia5-dev \
  libqt5multimedia5-plugins libmecab-dev mecab-ipadic-utf8 \
  fonts-noto-cjk gstreamer1.0-plugins-good
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/sing-japanese
```

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
app/                 # Qt Widgets 界面、MeCab 注音、振假名绘制与 PDF
tests/               # 注音、原文保留及 PDF 导出测试
public/audio/        # 离线假名录音，通过 Qt 资源嵌入程序
public/licenses/     # 词典许可声明
```

发音录音来自 Wikimedia Commons，录音者为 Hakatanoshio117117，按 Public Domain 发布。
来源和逐条校验信息见 [音频资源说明](public/RESOURCE-LICENSE.md)。
