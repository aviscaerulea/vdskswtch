# vim: set ft=ps1 fenc=utf-8 ff=unix sw=4 ts=4 et :
# ==================================================
# vdskswtch ビルドスクリプト
# MSVC cl.exe で src/*.cpp をコンパイルし out/vdskswtch.exe を生成する
#
# 引数:
#   -Version  : バージョン文字列（例: 1.0.0）
#   -Config   : Debug | Release（デフォルト: Debug）
# ==================================================
param(
    [string]$Version = "dev",
    [string]$Config  = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# VS 開発環境をロード（公式 DLL モジュール方式、Build Tools 単体環境にも対応）
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -products '*' -latest -property installationPath
if (-not $vsPath) { Write-Error "Visual Studio / Build Tools が見つからない"; exit 1 }

$devShellDll = Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (-not (Test-Path $devShellDll)) { Write-Error "DevShell.dll が見つからない: $devShellDll"; exit 1 }
Import-Module $devShellDll
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64"

# 出力ディレクトリ作成
if (-not (Test-Path "out")) { New-Item -ItemType Directory -Path "out" | Out-Null }

# コンパイルオプション
$commonFlags = @(
    "/utf-8", "/EHsc", "/std:c++17",
    "/W3",
    "/DAPP_VERSION=`"$Version`"",
    "/D_WIN32_WINNT=0x0A00", # Windows 10/11 対象
    "/Iinclude"              # toml++ ヘッダ
)

$debugFlags   = @("/Zi", "/Od", "/DDEBUG", "/MTd")
$releaseFlags = @("/O2", "/DNDEBUG", "/MT")

$configFlags = if ($Config -eq "Release") { $releaseFlags } else { $debugFlags }

# リンクライブラリ
$libs = @(
    "ole32.lib",        # COM 基盤（CoCreateInstance 等）
    "runtimeobject.lib", # Windows Runtime 文字列 API（WindowsCreateString 等）
    "user32.lib",       # ウィンドウ列挙 API（EnumWindows、IsWindowVisible 等）
    "windowsapp.lib",   # C++/WinRT（Toast 通知）
    "shell32.lib",      # IShellLinkW（ショートカット作成）
    "propsys.lib"       # IPropertyStore（AUMID 付与）
)

# リンクオプション
$linkFlags = @("/SUBSYSTEM:CONSOLE")
if ($Config -eq "Debug") { $linkFlags += "/DEBUG" }

$outExe = "out\vdskswtch.exe"

Write-Host "Building $outExe ($Config, v$Version)..."

# リソースコンパイル（アイコン埋め込み）
# rc.exe は src/ ディレクトリをカレントにして実行し、app.ico の相対パスを解決する
Push-Location "src"
& rc.exe /fo "..\out\app.res" "app.rc"
Pop-Location
if ($LASTEXITCODE -ne 0) { Write-Error "リソースコンパイル失敗 (exit $LASTEXITCODE)"; exit $LASTEXITCODE }

$clArgs = $commonFlags + $configFlags + @("src\main.cpp", "src\virtual_desktop.cpp", "src\config.cpp", "src\toast.cpp") + `
          @("out\app.res", "/Fe:$outExe", "/Fo:out\\") + @("/link") + $linkFlags + $libs

& cl.exe @clArgs
if ($LASTEXITCODE -ne 0) { Write-Error "ビルド失敗 (exit $LASTEXITCODE)"; exit $LASTEXITCODE }

Write-Host "Build succeeded: $outExe"
