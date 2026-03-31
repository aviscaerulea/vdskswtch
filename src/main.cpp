// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#include "virtual_desktop.hpp"
#include "config.hpp"
#include "toast.hpp"
#include <cstdio>
#include <cstring>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

// ミーティング用デスクトップのデフォルト名
static const wchar_t* DEFAULT_DESKTOP_NAME = L"Meeting";

// プロセス排他用 Named Mutex 名
static const wchar_t* MUTEX_NAME = L"Global\\vdskswtch_single_instance";

// Toast 通知表示後の待機時間（ms）
// プロセス終了前に OS が通知を表示する時間を確保する
static const DWORD TOAST_DISPLAY_WAIT_MS = 500;

static void print_usage()
{
    fprintf(stderr,
        "vdskswtch v" APP_VERSION " - Windows 11 Virtual Desktop Switcher\n"
        "\n"
        "Usage:\n"
        "  vdskswtch               ミーティング用デスクトップに切り替え（switch のエイリアス）\n"
        "  vdskswtch switch [name] ミーティング用デスクトップに切り替え（なければ作成）\n"
        "  vdskswtch close  [name] ミーティング用デスクトップを削除してメインに戻る\n"
        "  vdskswtch version       バージョンを表示\n"
        "  vdskswtch help          このヘルプを表示\n"
        "\n"
        "デスクトップ名のデフォルト: Meeting\n");
}

// switch サブコマンド
//
// 指定した名前の仮想デスクトップに切り替える。
// 存在しない場合は作成してから切り替える。
static int cmd_switch(const wchar_t* name, const Config& cfg)
{
    IVirtualDesktopManagerInternal* pManager = nullptr;
    HRESULT hr = InitVirtualDesktopManager(&pManager);
    if (FAILED(hr)) {
        return 1;
    }

    auto cleanup = [&](int result) {
        pManager->Release();
        return result;
    };

    IVirtualDesktop* pTarget = nullptr;
    hr = FindDesktopByName(pManager, name, &pTarget);
    if (FAILED(hr)) {
        return cleanup(1);
    }

    bool created = false;
    if (!pTarget) {
        hr = CreateNamedDesktop(pManager, name, &pTarget);
        if (FAILED(hr)) {
            return cleanup(1);
        }
        created = true;
    }

    if (!created) {
        IVirtualDesktop* pCurrent = nullptr;
        hr = pManager->GetCurrentDesktop(&pCurrent);
        if (SUCCEEDED(hr) && pCurrent) {
            GUID idCurrent = {}, idTarget = {};
            const bool ok = SUCCEEDED(pCurrent->GetId(&idCurrent))
                         && SUCCEEDED(pTarget->GetId(&idTarget));
            pCurrent->Release();

            if (ok && IsEqualGUID(idCurrent, idTarget)) {
                printf("デスクトップ \"%ls\" は既にアクティブです。\n", name);
                pTarget->Release();
                return cleanup(0);
            }
        }
    }

    ExecCommands(cfg.pre_exec);

    hr = SwitchToDesktop(pManager, pTarget);
    pTarget->Release();

    if (FAILED(hr)) {
        return cleanup(1);
    }

    if (created) {
        printf("デスクトップ \"%ls\" を作成して切り替えました。\n", name);
    }
    else {
        printf("デスクトップ \"%ls\" に切り替えました。\n", name);
    }

    PinAppWindows(cfg.pin_apps);

    ExecCommands(cfg.post_exec);

    return cleanup(0);
}

// close サブコマンド
//
// 指定した名前の仮想デスクトップを削除する。
// 削除後はメインデスクトップ（最初のデスクトップ）に移動する。
// close_exec はデスクトップ存在確認後、削除前に実行する。
static int cmd_close(const wchar_t* name, const Config& cfg)
{
    IVirtualDesktopManagerInternal* pManager = nullptr;
    HRESULT hr = InitVirtualDesktopManager(&pManager);
    if (FAILED(hr)) {
        return 1;
    }

    auto cleanup = [&](int result) {
        pManager->Release();
        return result;
    };

    // close_exec をデスクトップ不在時に実行しないよう事前確認する
    IVirtualDesktop* pTarget = nullptr;
    hr = FindDesktopByName(pManager, name, &pTarget);
    if (FAILED(hr)) {
        return cleanup(1);
    }
    if (!pTarget) {
        fprintf(stderr, "デスクトップ \"%ls\" が見つかりません。\n", name);
        return cleanup(1);
    }

    ExecCommands(cfg.close_exec);

    hr = RemoveDesktop(pManager, pTarget);
    pTarget->Release();
    if (FAILED(hr)) {
        return cleanup(1);
    }
    printf("デスクトップ \"%ls\" を削除しました。\n", name);
    return cleanup(0);
}

int wmain(int argc, wchar_t* argv[])
{
    // version/help は排他制御・COM 初期化不要
    if (argc >= 2) {
        const wchar_t* cmd = argv[1];

        if (wcscmp(cmd, L"version") == 0) {
            printf("vdskswtch v" APP_VERSION "\n");
            return 0;
        }
        if (wcscmp(cmd, L"help") == 0 || wcscmp(cmd, L"--help") == 0 || wcscmp(cmd, L"-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // COM 初期化（STA、プロセス全体で一度だけ）
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "CoInitializeEx 失敗: 0x%08lX\n", hr);
        return 1;
    }

    // Toast 通知の基盤初期化（AUMID 設定 + ショートカット作成）
    InitToast();

    // プロセス排他チェック
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, MUTEX_NAME);
    if (!hMutex) {
        fprintf(stderr, "Mutex の作成に失敗しました: 0x%08lX\n", GetLastError());
        CoUninitialize();
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ShowToast(L"vdskswtch は既に実行中です。");
        Sleep(TOAST_DISPLAY_WAIT_MS);
        CloseHandle(hMutex);
        CoUninitialize();
        return 1;
    }

    int result = 1;

    if (argc < 2) {
        Config cfg = LoadConfig();
        result = cmd_switch(DEFAULT_DESKTOP_NAME, cfg);
    }
    else {
        const wchar_t* cmd = argv[1];

        if (wcscmp(cmd, L"switch") == 0) {
            Config cfg = LoadConfig();
            const wchar_t* name = (argc >= 3) ? argv[2] : DEFAULT_DESKTOP_NAME;
            result = cmd_switch(name, cfg);
        }
        else if (wcscmp(cmd, L"close") == 0) {
            Config cfg = LoadConfig();
            const wchar_t* name = (argc >= 3) ? argv[2] : DEFAULT_DESKTOP_NAME;
            result = cmd_close(name, cfg);
        }
        else {
            fprintf(stderr, "不明なコマンドです: %ls\n\n", cmd);
            print_usage();
        }
    }

    CloseHandle(hMutex);
    CoUninitialize();
    return result;
}
