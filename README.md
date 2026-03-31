# vdskswtch

Windows 11 の仮想デスクトップを切り替える CLI ツール。

メインとミーティング用の 2 つの仮想デスクトップを管理する。
ミーティング用デスクトップが存在しない場合は自動作成して切り替え、
不要になったら削除してメインに戻る。

## 動作要件

- Windows 11 24H2（ビルド 26100 以降）

## インストール

[Releases](https://github.com/your-repo/vdskswtch/releases) から最新の zip を取得し、
`vdskswtch.exe` をパスの通ったディレクトリに配置する。

## 使い方

```
vdskswtch               ミーティング用デスクトップに切り替え（switch のエイリアス）
vdskswtch switch [name] ミーティング用デスクトップに切り替え（なければ作成）
vdskswtch close  [name] ミーティング用デスクトップを削除してメインに戻る
vdskswtch version       バージョンを表示
vdskswtch help          ヘルプを表示
```

デスクトップ名のデフォルトは `Meeting`。`name` で任意の名前を指定できる。

## 使用例

```bash
# ミーティング用デスクトップに切り替え（なければ "Meeting" という名前で作成）
vdskswtch

# 同上（明示的に switch コマンドを使う場合）
vdskswtch switch

# "Work" という名前のデスクトップに切り替え（なければ作成）
vdskswtch switch Work

# ミーティング用デスクトップを削除してメインに戻る
vdskswtch close
```

## 設定ファイル

`vdskswtch.exe` と同じディレクトリに `vdskswtch.toml` を配置すると、
デスクトップ切り替え時にスクリプトを自動実行できる。

```toml
# switch 前に実行するコマンド
pre_exec = []

# switch 後に実行するコマンド
post_exec = ["C:\\scripts\\start-meeting.bat"]

# close 時（デスクトップ削除前）に実行するコマンド
close_exec = ["C:\\scripts\\end-meeting.bat"]

# 全仮想デスクトップにピン留めするプロセス名（大文字小文字不問）
# 注意：ウィンドウ単位でのピン留めのため、アプリ再起動後は新しいウィンドウのピン留めが解除される
pin_apps = ["Teams.exe", "Zoom.exe"]
```

`vdskswtch.local.toml` を配置すると `vdskswtch.toml` の設定を上書きできる。

## 注意事項

- undocumented な Windows 内部 API を使用しているため、Windows Update により動作しなくなる場合がある。
- switch/close コマンドは同時に 1 つのインスタンスのみ実行できる。既に実行中の場合は Toast 通知を表示して終了する。
