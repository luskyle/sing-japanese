# 假名小调 · Sing Japanese

用声音建立假名记忆的离线桌面应用。技术栈为 **Rust + Tauri 2 + HTML/CSS/TypeScript（Vite）**，
界面为中文；没有在线语音服务、账号或学习数据上传。

## 第一阶段

- 平假名／片假名切换；清音 46、浊音 20、半浊音 5。
- 71 条已下载的真人发音，两种字形共用录音；支持原速和保持音高的 0.8 倍速播放。
- 点击假名听音、按行学习、隐藏罗马音、查看发音提示。
- 听音选字：优先练习不熟悉或容易出错的字，避免立即重复目标字。
- 同音的 o/wo、ji/ぢ、zu/づ 不会被放在同一道选择题中互相竞争。
- 分别记录平、片假名的听音次数、答题次数、正确率和连续答对次数。
- Rust 将桌面版进度保存到应用数据目录，使用临时文件原子替换；错误会在界面提示。
- 字体、图标、音频均随应用保存，运行时不需要网络。

音频通过本地 fetch 读取为 Blob 后播放，并允许 CSP 的 `media-src blob:`，
以兼容 Linux WebKit 无法可靠直接播放 `tauri://` 媒体地址的情况。
切换发音会取消旧请求并释放旧的 Blob URL，避免重叠播放。

## 歌词自动注音

应用默认进入卡片式首页：第一张卡片引导学习假名，第二张核心功能卡片进入歌词注音；
听辨练习、学习指南和本地资源也可从首页进入。功能页通过“返回首页”或顶部品牌回到首页，
不再使用左侧标签导航，也不强制先完成假名课程。

桌面版首页打开“歌词注音”，粘贴日语文本并点击“自动注音”，即可在汉字词上方显示平假名。
支持显示／隐藏振假名，保留原始文字、空格及换行；一次最多 20,000 个 Unicode 字符。
不需要人工修正或大模型，输入与结果仅保存在本次会话内，不上传、不自动存盘。

Rust 使用 Lindera 0.43.3 和嵌入的 IPADIC 词典，按词输出常规读音（包括词内送假名）。
未知汉字词保留原文并以虚线提示；人名、造词和歧义读法可能有误，不匹配歌手特殊唱法。
词典在首次注音时初始化，分析在线程池运行，不阻塞桌面 UI。
浏览器预览没有 Rust 后端，会明确提示改用桌面版，不提供假结果。

IPADIC 首次编译需要联网获取原始词典，后续运行无需网络；CI 的 Cargo 缓存包含构建产物。
原始词典再分发声明随应用保存在
[`public/licenses/IPADIC-NOTICE.txt`](./public/licenses/IPADIC-NOTICE.txt)，应用内“本地资源”可展开查看。

目前不包含歌曲音频导入、书写识别、口语评分或完整的拗音／促音／长音音频课程。
“连续答对 3 次”仅为熟悉度提示，不是长期记忆或发音准确性的证明。

## 开发

需要 Node.js 20.19+（或 22.12+）、npm、Rust 1.89+。依赖锁文件已保存在项目中；
Rust 依赖解析优先选择与最低 Rust 版本兼容的版本，Tauri 的 JS API 与 Rust 保持 2.11 系列。

Ubuntu / Debian 的原生开发依赖参考 [Tauri 官方说明](https://v2.tauri.app/start/prerequisites/)：

```sh
sudo apt install libwebkit2gtk-4.1-dev build-essential curl wget file \
  libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev
npm ci
npm run desktop
```

仅预览 H5 界面：

```sh
npm run dev
# http://127.0.0.1:1420
```

浏览器预览的进度使用 localStorage，**不与桌面版互通**。清理浏览器网站数据会删除预览版记录。
浏览器预览期间必须保持 `npm run dev` 运行；关闭服务后，已打开的页面无法继续加载录音，
会提示“录音加载失败”。重新启动服务后可以直接重试播放；打包后的桌面版不依赖此服务。
桌面版 Linux 进度通常位于
`~/.local/share/com.luskyle.sing-japanese/progress.json`（遵循 `XDG_DATA_HOME`）。
若记录损坏，应用会暂停练习并显示错误，不会静默覆盖；请先备份该文件再处理。
无需写入项目目录，因此安装后也可以保存进度。

## 构建桌面包

```sh
npm run desktop:build
```

Linux 可执行文件：`src-tauri/target/release/sing-japanese`。
Debian 安装包：`src-tauri/target/release/bundle/deb/`。
安装后的桌面入口名为“假名小调”。其他平台需在对应系统配置工具链后构建，尚未验证。

## GitHub CI / 发布

推送 `main`、创建 PR 或手动运行 **Desktop CI** 时，先执行前端测试、离线音频校验、
生产构建和 Chromium 测试，再进行三平台 Rust 测试、Clippy 和安装包构建：

| 平台                        | 安装包                  |
| --------------------------- | ----------------------- |
| Linux x64（Ubuntu 22.04）   | `.deb`、`.AppImage` |
| Windows x64                 | NSIS `.exe`           |
| macOS Intel + Apple Silicon | Universal `.dmg`      |

每个平台的安装包作为 Actions artifact 保留 14 天。Linux 还会通过虚拟显示和音频设备
测试实际桌面应用的发音播放、Rust 保存和重启恢复。CI 使用仓库中已保存的音频，不重新下载教学素材。

发布步骤：

1. 同步修改 `package.json`、`package-lock.json`、`src-tauri/Cargo.toml`、
   `src-tauri/Cargo.lock` 中本项目版本及 `src-tauri/tauri.conf.json` 的版本。
2. 提交并推送代码，然后推送匹配的标签，例如 `git tag v0.1.0 && git push origin v0.1.0`。
3. **Desktop Release** 检查标签与版本一致，复用整套 CI；所有平台成功后才创建 Release 草稿，
   附四个安装包与 `SHA256SUMS.txt`。
4. 在 GitHub Releases 检查草稿和安装包，再点击发布。也可在标签对应的 ref 上手动运行该工作流；
   在分支上运行会明确失败。已有同名 Release 不会自动覆盖。

当前发布不需要手动添加仓库 secrets，只有创建草稿的 job 拥有 `contents: write`；
构建 job 不接收签名密钥，第三方 Actions 固定到提交 SHA。
Windows / macOS 安装包暂未代码签名，macOS 未公证，系统可能显示安全警告；
SHA-256 仅验证文件完整性，不代替签名。当前没有应用内自动更新功能。

如果此工作区曾使用 `.native-deps/` 局部解包开发依赖，可在同一台机器临时执行：

```sh
PKG_CONFIG_PATH="$PWD/.native-deps/root/usr/lib/x86_64-linux-gnu/pkgconfig" npm run desktop
```

该目录不纳入版本控制，不是项目的正常安装方式；换机应安装官方开发依赖。
图形桌面需能使用 GTK / WebKitGTK，并有系统音频输出。

## 本地资源

音频保存在 [`public/audio/`](./public/audio/)，逐条来源、许可、大小和 SHA-256
保存在 [`public/audio-manifest.json`](./public/audio-manifest.json)。

录音者 **Hakatanoshio117117** 在 Wikimedia Commons 以公有领域方式发布原始录音。
详见 [`public/RESOURCE-LICENSE.md`](./public/RESOURCE-LICENSE.md)。
日文字体为 Noto Sans CJK JP 的假名及「音」字子集，© 2014–2021 Adobe，
采用 SIL OFL 1.1，见 [`public/fonts/OFL.txt`](./public/fonts/OFL.txt)。
字体子集保留了原始版权元数据；仅用于界面的假名字形，不替代中文系统字体。

正常安装、构建、播放不需要重新下载音频。重新获取原始资源需要联网及 curl：

```sh
npm run resources:download
npm run resources:verify  # 需要 ffprobe，通常由 ffmpeg 软件包提供
```

下载脚本只接受原始元数据中标为 Public domain 的文件，不使用“可免费试听”代替再分发许可。
校验会检查 71 个文件的 SHA-256、许可、来源、Ogg 文件头、解码和时长。

## 验证

```sh
npm test
npm run build
npm run resources:verify
npm exec playwright -- install chromium  # 首次浏览器测试前
npm run test:e2e
cargo test --manifest-path src-tauri/Cargo.toml
cargo clippy --manifest-path src-tauri/Cargo.toml -- -D warnings
```

测试覆盖字形映射、同音字排除、答题计分、范围切换、记录恢复、损坏记录保护、
音频失败恢复、离线请求及全部 71 条录音的浏览器实际解码。
这些测试不代替真人对日语发音和教学效果的评估。

真实桌面集成测试（Linux，需要图形会话、已构建 release 应用、
[tauri-driver](https://v2.tauri.app/develop/tests/webdriver/) 和 WebKitWebDriver）：

```sh
cargo install tauri-driver --version 2.0.6 --locked  # 兼容 Rust 1.89
sudo apt install webkit2gtk-driver
npm run test:desktop
```

可通过 `TAURI_DRIVER` / `WEBKIT_WEBDRIVER` 指定驱动路径，通过 `TAURI_TEST_PORT`
指定端口（默认使用 4444 和 4445）。测试在独立临时数据目录中启动实际桌面应用，
验证本地录音播放、Rust IPC 保存、关闭重启恢复和听辨答题；不会改写你的正式学习记录。
