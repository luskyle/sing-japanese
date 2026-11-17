# Bundled third-party components

The Windows and macOS distributions contain dynamically linked Qt 5.15.2,
MeCab, Minizip, zlib, a compiled UTF-8 IPADIC dictionary, Noto Sans CJK JP,
and the application's original kana recordings.

- Qt: LGPL v3, with applicable Qt exceptions. Source for this exact Qt release:
  <https://download.qt.io/archive/qt/5.15/5.15.2/single/>.
  The Qt libraries are unmodified. You may replace/relink the dynamically loaded
  libraries with compatible modified versions; reverse engineering for debugging
  modifications to these libraries is not restricted.
- MeCab: BSD license option, <https://github.com/taku910/mecab>.
- Minizip and zlib: zlib license, <https://github.com/madler/zlib>.
- IPADIC: NAIST license; see `IPADIC-NOTICE.txt` and `IPADIC-copyright`.
- Noto Sans CJK JP: SIL Open Font License 1.1; see `Noto-OFL.txt`.
- Kana recordings: Public Domain; see `RESOURCE-LICENSE.md` and
  `audio-manifest.json` for attribution and original sources.

Full component license notices accompany this file. Linux packages use system
Qt, MeCab, dictionary, and compression libraries instead of bundling them.

Windows packages are unsigned. The macOS application is ad-hoc signed to seal
the bundle, not Developer ID signed or notarized. No publisher identity is
asserted; verify the release checksums before opening.
