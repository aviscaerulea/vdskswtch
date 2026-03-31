// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#pragma once

#include <windows.h>
#include <shobjidl_core.h>
#include <winstring.h>
#include <string>
#include <vector>

// ==================================================
// Windows 11 24H2（ビルド 26100 以降）向け
// undocumented COM インターフェース定義
//
// 各 GUID と vtable 順序は MScholtes/VirtualDesktop の
// VirtualDesktop11-24H2.cs に基づく。
// Windows Update により変更される可能性がある。
// ==================================================

// CLSID_ImmersiveShell
// CoCreateInstance でシェルサービスプロバイダを取得するための CLSID
static const CLSID CLSID_ImmersiveShell = {
    0xC2F03A33, 0x21F5, 0x47FA, { 0xB4, 0xBB, 0x15, 0x63, 0x62, 0xA2, 0xF2, 0x39 }
};

// CLSID_VirtualDesktopManagerInternal
// IServiceProvider::QueryService で IVirtualDesktopManagerInternal を取得するための CLSID
static const CLSID CLSID_VirtualDesktopManagerInternal = {
    0xC5E0CDCA, 0x7B6E, 0x41B2, { 0x9F, 0xC4, 0xD9, 0x39, 0x75, 0xCC, 0x46, 0x7B }
};

// IVirtualDesktop（IID: 3F07F4BE-B107-441A-AF0F-39D82529072C）
// 1 つの仮想デスクトップを表す COM インターフェース
MIDL_INTERFACE("3F07F4BE-B107-441A-AF0F-39D82529072C")
IVirtualDesktop : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsViewVisible(IUnknown* pView, BOOL* pfVisible) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetId(GUID* pGuid) = 0;

    // デスクトップ名を HSTRING で返す。呼び出し元が WindowsDeleteString で解放する
    virtual HRESULT STDMETHODCALLTYPE GetName(HSTRING* pName) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWallpaperPath(HSTRING* pPath) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemote(BOOL* pfRemote) = 0;
};

// IVirtualDesktopManagerInternal（IID: 53F5CA0B-158F-4124-900C-057158060B27）
// 仮想デスクトップの列挙・切り替え・作成・削除・名前設定を行う内部 COM インターフェース
//
// vtable 順序は Windows 11 24H2 専用。
// メソッド順序がビルドにより変わることがあるため、OS バージョン確認を推奨する。
MIDL_INTERFACE("53F5CA0B-158F-4124-900C-057158060B27")
IVirtualDesktopManagerInternal : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT* pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(IUnknown* pView, IVirtualDesktop* pDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(IUnknown* pView, BOOL* pfCanMove) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentDesktop(IVirtualDesktop** ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDesktops(IObjectArray** ppDesktops) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAdjacentDesktop(IVirtualDesktop* pDesktopFrom, UINT uDirection, IVirtualDesktop** ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktop(IVirtualDesktop* pDesktop) = 0;

    // Windows 11 24H2 で追加。SwitchDesktop と CreateDesktop の間に入る
    virtual HRESULT STDMETHODCALLTYPE SwitchDesktopAndMoveForegroundView(IVirtualDesktop* pDesktop) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateDesktop(IVirtualDesktop** ppDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveDesktop(IVirtualDesktop* pDesktop, INT nIndex) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveDesktop(IVirtualDesktop* pDesktopRemove, IVirtualDesktop* pDesktopFallback) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindDesktop(const GUID* pGuid, IVirtualDesktop** ppDesktop) = 0;

    // vtable 位置 12（SetDesktopName の前にあるダミーメソッド）
    virtual HRESULT STDMETHODCALLTYPE GetDesktopSwitchIncludeExcludeViews(
        IVirtualDesktop* pDesktop, IObjectArray** ppUnknown1, IObjectArray** ppUnknown2) = 0;

    // デスクトップ名を設定する。name は HSTRING（WindowsCreateString で作成）
    virtual HRESULT STDMETHODCALLTYPE SetDesktopName(IVirtualDesktop* pDesktop, HSTRING name) = 0;
};

// CLSID_VirtualDesktopPinnedApps
// IServiceProvider::QueryService で IVirtualDesktopPinnedApps を取得するための CLSID
static const CLSID CLSID_VirtualDesktopPinnedApps = {
    0xB5A399E7, 0x1C87, 0x46B8, { 0x88, 0xE9, 0xFC, 0x57, 0x47, 0xB1, 0x71, 0xBD }
};

// CLSID_ApplicationViewCollection
// IServiceProvider::QueryService で IApplicationViewCollection を取得するための CLSID
static const CLSID CLSID_ApplicationViewCollection = {
    0x1841C6D7, 0x4F9D, 0x42C0, { 0xAF, 0x41, 0x87, 0x47, 0x53, 0x8F, 0x10, 0xE5 }
};

// IApplicationView（IID: 372E1D3B-38D6-45A8-956B-0A6B69186A21）
// 1 つのアプリケーションウィンドウビューを表す COM インターフェース
// PinView に渡すオペーク型として使用する
MIDL_INTERFACE("372E1D3B-38D6-45A8-956B-0A6B69186A21")
IApplicationView : public IUnknown {
};

// IApplicationViewCollection（IID: 1841C6D7-4F9D-42C0-AF41-8747538F10E5）
// HWND から IApplicationView を取得するための COM インターフェース
//
// vtable 順序は Windows 11 24H2 専用。
// メソッド順序がビルドにより変わることがあるため、OS バージョン確認を推奨する。
MIDL_INTERFACE("1841C6D7-4F9D-42C0-AF41-8747538F10E5")
IApplicationViewCollection : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetViews(IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByZOrder(IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewsByAppUserModelId(PCWSTR, IObjectArray**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetViewForHwnd(HWND hwnd, IApplicationView** ppView) = 0;
};

// IVirtualDesktopPinnedApps（IID: 4CE81583-1E4C-4632-A621-07A53543148F）
// ウィンドウを全仮想デスクトップにピン留め/解除する COM インターフェース
//
// vtable 順序は Windows 11 24H2 専用。
// メソッド順序がビルドにより変わることがあるため、OS バージョン確認を推奨する。
MIDL_INTERFACE("4CE81583-1E4C-4632-A621-07A53543148F")
IVirtualDesktopPinnedApps : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsViewPinned(IApplicationView* pView, BOOL* pfPinned) = 0;
    virtual HRESULT STDMETHODCALLTYPE PinView(IApplicationView* pView) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnpinView(IApplicationView* pView) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsAppIdPinned(PCWSTR appId, BOOL* pfPinned) = 0;
    virtual HRESULT STDMETHODCALLTYPE PinAppID(PCWSTR appId) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnpinAppID(PCWSTR appId) = 0;
};

// ==================================================
// 操作関数
// ==================================================

// IVirtualDesktopManagerInternal を取得する
//
// COM は呼び出し元が STA で初期化済みであること。
// 成功時: S_OK を返し *ppManager に取得したポインタを格納する。
// 失敗時: HRESULT エラーコードを返す。
// 呼び出し元は ppManager->Release() を呼ぶ責任を持つ。
HRESULT InitVirtualDesktopManager(IVirtualDesktopManagerInternal** ppManager);

// 名前が一致する仮想デスクトップを検索する
//
// 見つかった場合: S_OK を返し *ppDesktop にポインタを格納する
// 見つからない場合: S_OK を返し *ppDesktop = nullptr とする
// 呼び出し元は取得した IVirtualDesktop* を Release() する責任を持つ
HRESULT FindDesktopByName(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name,
    IVirtualDesktop** ppDesktop);

// 仮想デスクトップを作成して名前を設定する
//
// 成功時: S_OK を返し *ppDesktop に新しいデスクトップのポインタを格納する
// 呼び出し元は取得した IVirtualDesktop* を Release() する責任を持つ
HRESULT CreateNamedDesktop(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name,
    IVirtualDesktop** ppDesktop);

// 指定した仮想デスクトップに切り替える
HRESULT SwitchToDesktop(
    IVirtualDesktopManagerInternal* pManager,
    IVirtualDesktop* pDesktop);

// 指定した仮想デスクトップを削除する
//
// デスクトップが 1 つしかない場合: E_FAIL を返す
// 削除時は最初の（メインの）デスクトップを fallback に設定する
// pDesktop の Release は呼び出し元の責任
HRESULT RemoveDesktop(
    IVirtualDesktopManagerInternal* pManager,
    IVirtualDesktop* pDesktop);

// 指定した exe 名のプロセスのウィンドウを全仮想デスクトップにピン留めする
//
// EnumWindows で各ウィンドウの所属プロセスを検査し、
// pin_apps に一致するプロセスのウィンドウを IVirtualDesktopPinnedApps::PinView でピン留めする。
// 既にピン留め済みのウィンドウはスキップする。失敗したウィンドウは警告を出力して継続する。
// COM は呼び出し元が初期化済みである前提で動作する。
HRESULT PinAppWindows(const std::vector<std::wstring>& pin_apps);
