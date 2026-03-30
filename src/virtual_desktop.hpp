// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#pragma once

#include <windows.h>
#include <shobjidl_core.h>
#include <winstring.h>
#include <string>

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

// ==================================================
// 操作関数
// ==================================================

// COM を初期化し IVirtualDesktopManagerInternal を取得する
//
// 成功時: S_OK を返し *ppManager に取得したポインタを格納する
// 失敗時: HRESULT エラーコードを返す
// 呼び出し元は CoUninitialize() と ppManager->Release() を呼ぶ責任を持つ
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

// 名前が一致する仮想デスクトップを削除する
//
// 見つからない場合: E_FAIL を返す
// 削除時は最初の（メインの）デスクトップを fallback に設定する
HRESULT RemoveDesktopByName(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name);
