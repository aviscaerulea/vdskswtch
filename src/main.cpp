// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#include "virtual_desktop.hpp"
#include "config.hpp"
#include <cstdio>
#include <cstring>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

// ミーティング用デスクトップのデフォルト名
static const wchar_t* DEFAULT_DESKTOP_NAME = L"Meeting";

static void print_usage()
{
    fprintf(stderr,
        "vdsktop v" APP_VERSION " - Windows 11 Virtual Desktop Switcher\n"
        "\n"
        "Usage:\n"
        "  vdsktop               ミーティング用デスクトップに切り替え（switch のエイリアス）\n"
        "  vdsktop switch [name] ミーティング用デスクトップに切り替え（なければ作成）\n"
        "  vdsktop close  [name] ミーティング用デスクトップを削除してメインに戻る\n"
        "  vdsktop version       バージョンを表示\n"
        "  vdsktop help          このヘルプを表示\n"
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

    // 単一出口でリソースを解放するためのクリーンアップラムダ
    auto cleanup = [&](int result) {
        pManager->Release();
        CoUninitialize();
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
            pCurrent->GetId(&idCurrent);
            pTarget->GetId(&idTarget);
            pCurrent->Release();

            if (IsEqualGUID(idCurrent, idTarget)) {
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
        CoUninitialize();
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
    pTarget->Release();

    ExecCommands(cfg.close_exec);

    hr = RemoveDesktopByName(pManager, name);
    if (FAILED(hr)) {
        return cleanup(1);
    }
    printf("デスクトップ \"%ls\" を削除しました。\n", name);
    return cleanup(0);
}

int wmain(int argc, wchar_t* argv[])
{
    // 引数なし: switch のエイリアス
    if (argc < 2) {
        Config cfg = LoadConfig();
        return cmd_switch(DEFAULT_DESKTOP_NAME, cfg);
    }

    const wchar_t* cmd = argv[1];

    if (wcscmp(cmd, L"switch") == 0) {
        Config cfg = LoadConfig();
        const wchar_t* name = (argc >= 3) ? argv[2] : DEFAULT_DESKTOP_NAME;
        return cmd_switch(name, cfg);
    }

    if (wcscmp(cmd, L"close") == 0) {
        Config cfg = LoadConfig();
        const wchar_t* name = (argc >= 3) ? argv[2] : DEFAULT_DESKTOP_NAME;
        return cmd_close(name, cfg);
    }

    if (wcscmp(cmd, L"version") == 0) {
        printf("vdsktop v" APP_VERSION "\n");
        return 0;
    }

    if (wcscmp(cmd, L"help") == 0 || wcscmp(cmd, L"--help") == 0 || wcscmp(cmd, L"-h") == 0) {
        print_usage();
        return 0;
    }

    fprintf(stderr, "不明なコマンドです: %ls\n\n", cmd);
    print_usage();
    return 1;
}
