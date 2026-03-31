// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#pragma once

// Toast 通知の初期化
//
// AUMID を設定し、スタートメニューにショートカットを作成する。
// COM が STA で初期化済みであること。
void InitToast();

// Toast 通知を表示する
//
// OS に通知を登録して即 return する。
// 失敗した場合は stderr に警告を出力するが、呼び出し元の処理は妨げない。
void ShowToast(const wchar_t* message);
