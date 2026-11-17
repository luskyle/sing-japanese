$ErrorActionPreference = 'Stop'
$version = [regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(sing_japanese VERSION ([0-9.]+)').Groups[1].Value
if (-not $version) { throw 'Cannot determine application version' }
$stage = Join-Path $PWD 'build/portable/SingJapanese'
cmake --install build --prefix $stage
if ($LASTEXITCODE) { throw 'Installation failed' }
Get-ChildItem "$stage/bin" | Move-Item -Destination $stage
Remove-Item "$stage/bin"
Copy-Item "$env:VCPKG_INSTALLATION_ROOT/installed/x64-windows/bin/*.dll" $stage
Copy-Item runtime/licenses "$stage/licenses" -Recurse
windeployqt --release --no-translations --compiler-runtime --qmldir app/qml "$stage/sing-japanese.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
$crt = Get-ChildItem "$env:VCToolsRedistDir/x64" -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw 'Cannot find the redistributable MSVC runtime' }
Copy-Item "$($crt.FullName)/*.dll" $stage
@'
[Paths]
Prefix=.
Plugins=.
Qml2Imports=qml
'@ | Set-Content "$stage/qt.conf" -Encoding ascii
$oldPath = $env:PATH
$env:PATH = "$env:SystemRoot/system32;$env:SystemRoot"
$env:QT_PLUGIN_PATH = ''
$env:QML2_IMPORT_PATH = ''
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QUICK_BACKEND = 'software'
try {
    $process = Start-Process "$stage/sing-japanese.exe" -ArgumentList '--smoke-test' -PassThru
    if (-not $process.WaitForExit(30000)) {
        Stop-Process -Id $process.Id
        throw 'Packaged application smoke test timed out'
    }
    if ($process.ExitCode -ne 0) { throw "Packaged application failed: $($process.ExitCode)" }
} finally {
    $env:PATH = $oldPath
}
New-Item -ItemType Directory -Force release | Out-Null
Compress-Archive -Path $stage -DestinationPath "release/sing-japanese-$version-Windows-x86_64.zip"
