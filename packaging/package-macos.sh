#!/usr/bin/env bash
set -euo pipefail

version="$(sed -n 's/^project(sing_japanese VERSION \([^ )]*\).*/\1/p' CMakeLists.txt)"
test -n "$version"
stage="$PWD/build/dmg"
mkdir -p "$stage" release
cp -R build/sing-japanese.app "$stage/唱日语.app"
app="$stage/唱日语.app"
mkdir -p "$app/Contents/Resources/licenses"
cp -R runtime/licenses/. "$app/Contents/Resources/licenses/"
macdeployqt "$app" -qmldir="$PWD/app/qml" -always-overwrite

# Fail if deployment left a dependency on the build machine's Homebrew paths.
while IFS= read -r -d '' binary; do
  if file "$binary" | grep -q 'Mach-O'; then
    if otool -L "$binary" | tail -n +2 | grep -Eq '/(usr/local|opt/homebrew|Users/runner)/'; then
      echo "Unbundled dependency: $binary" >&2
      otool -L "$binary" >&2
      exit 1
    fi
  fi
done < <(find "$app" -type f -print0)

codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
env -u DYLD_LIBRARY_PATH -u QT_PLUGIN_PATH -u QML2_IMPORT_PATH \
  QT_QPA_PLATFORM=cocoa QT_QUICK_BACKEND=software \
  "$app/Contents/MacOS/sing-japanese" --smoke-test
ln -s /Applications "$stage/Applications"
hdiutil create -volname "唱日语" -srcfolder "$stage" -ov -format UDZO \
  "release/sing-japanese-$version-macOS-x86_64.dmg"
