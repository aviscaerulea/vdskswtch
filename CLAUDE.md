# CLAUDE.md

## 開発環境

- Visual Studio Build Tools 2022 以降（または Visual Studio 2022 以降）
- Windows 11 24H2（ビルド 26100 以降）

## ビルド方法

```bash
task build    # デバッグビルド（out/vdsktop.exe）
task release  # リリースビルド + zip
task clean    # out/ 削除
```

## テスト方法

```bash
out/vdsktop.exe               # Meeting デスクトップに切り替え（なければ作成）
out/vdsktop.exe switch        # 同上
out/vdsktop.exe switch Work   # Work デスクトップに切り替え
out/vdsktop.exe close         # Meeting デスクトップを削除
out/vdsktop.exe version       # バージョン確認
```

## 実装上の注意点

### COM インターフェース

`IVirtualDesktopManagerInternal` は undocumented な COM インターフェースであり、
Windows Update により vtable レイアウトや GUID が変更される可能性がある。

- 対象 GUID は `src/virtual_desktop.hpp` に定義
- MScholtes/VirtualDesktop の `VirtualDesktop11-24H2.cs` を vtable 定義の根拠とする
- Windows 11 24H2 以外では動作保証なし

### ビルドの注意

- HSTRING 操作（`WindowsCreateString` 等）に `runtimeobject.lib` が必要
- COM 初期化には `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)` を使用
- `wmain` エントリポイントを使用（Unicode 引数対応）

## 参考

@README.md
