# 假名发音资源

本项目的 71 个 Ogg 真人发音片段来自 Wikimedia Commons。
录音者：**Hakatanoshio117117**。录音者以 **PD-self（公有领域）** 发布。
下载文件保持原样，没有剪辑、合成、变速或改音高。

- 原始集合：<https://commons.wikimedia.org/wiki/Category:Audio_files_of_hiragana_(set_by_Hakatanoshio117117)>
- 「そ」：<https://commons.wikimedia.org/wiki/File:Ja-So.oga>
- PD-self 声明：<https://commons.wikimedia.org/wiki/Template:PD-self>
- 逐条原始页面、下载地址、作者、许可、大小、SHA-256：
  [audio-manifest.json](./audio-manifest.json)

平假名和对应片假名共用同一发音文件。转写采用便于初学者的 Hepburn 风格；
文件 ID `di` / `du` 分别标识「ぢ」「づ」，并非英语 di / du 发音。
「を」的转写为 wo，现代标准语通常发音为 o。
听音选择题不把 o/wo、ji/di、zu/du 作为互相竞争的选项。

这些是孤立假名的参考录音，不是连续语流、方言、音高重音或歌唱发音课程。
部分录音的响度和时长有差异。未收录拗音、促音及长音的专门示范录音。
应用内的学习提示由本项目编写，不是转载原网站的教程。

重新下载：`npm run resources:download`（需要联网、会刷新 manifest）。
本地校验：`npm run resources:verify`（需要 ffprobe）。

## 离线字体

`fonts/kana.woff2` 为 Noto Sans CJK JP Regular 的假名和「音」字子集，
由 fontTools 从 NotoSansCJK-Regular.ttc 的 JP 字体（索引 0）生成：

```sh
pyftsubset NotoSansCJK-Regular.ttc --font-number=0 \
  --unicodes=U+3040-30FF,U+97F3 --flavor=woff2 --output-file=kana.woff2
```

原字体：© 2014–2021 Adobe (<http://www.adobe.com/>)。
源项目：<https://github.com/notofonts/noto-cjk>。
许可：SIL Open Font License 1.1，完整条款保存在 `fonts/OFL.txt`。
子集保留了原字体的版权和许可元数据。界面图标由本项目创建。
