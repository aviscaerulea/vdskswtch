// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#pragma once

#include <string>
#include <vector>

// vdsktop 設定
// vdsktop.toml → vdsktop.local.toml の順に読み込み、後者で上書きする
struct Config {
    // switch 前に実行するコマンド一覧
    std::vector<std::wstring> pre_exec;

    // switch 後に実行するコマンド一覧
    std::vector<std::wstring> post_exec;

    // close 時（デスクトップ削除前）に実行するコマンド一覧
    std::vector<std::wstring> close_exec;
};

// exe と同じディレクトリから設定を読み込む
//
// vdsktop.toml が存在しない場合は空の Config を返す。
// vdsktop.local.toml が存在すれば上書きマージする。
// パースエラー時は stderr に警告を出力し、読み込めた範囲の値を返す。
Config LoadConfig();

// コマンド配列を順に実行する
//
// 各コマンドを CreateProcessW で起動し、完了を待機する。
// 失敗したコマンドがあれば stderr に警告を出力するが、後続は継続する。
void ExecCommands(const std::vector<std::wstring>& commands);
