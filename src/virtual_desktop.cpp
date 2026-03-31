// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#include "virtual_desktop.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

HRESULT InitVirtualDesktopManager(IVirtualDesktopManagerInternal** ppManager)
{
    IServiceProvider* pServiceProvider = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_ImmersiveShell, nullptr, CLSCTX_ALL,
        __uuidof(IServiceProvider), reinterpret_cast<void**>(&pServiceProvider));
    if (FAILED(hr)) {
        fprintf(stderr, "ImmersiveShell の取得失敗: 0x%08lX\n", hr);
        return hr;
    }

    hr = pServiceProvider->QueryService(
        CLSID_VirtualDesktopManagerInternal,
        __uuidof(IVirtualDesktopManagerInternal),
        reinterpret_cast<void**>(ppManager));
    pServiceProvider->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "IVirtualDesktopManagerInternal の取得失敗: 0x%08lX\n", hr);
        return hr;
    }

    return S_OK;
}

HRESULT FindDesktopByName(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name,
    IVirtualDesktop** ppDesktop)
{
    *ppDesktop = nullptr;

    IObjectArray* pArray = nullptr;
    HRESULT hr = pManager->GetDesktops(&pArray);
    if (FAILED(hr)) {
        fprintf(stderr, "GetDesktops 失敗: 0x%08lX\n", hr);
        return hr;
    }

    UINT count = 0;
    hr = pArray->GetCount(&count);
    if (FAILED(hr)) {
        pArray->Release();
        return hr;
    }

    for (UINT i = 0; i < count; i++) {
        IVirtualDesktop* pDesktop = nullptr;
        hr = pArray->GetAt(i, __uuidof(IVirtualDesktop), reinterpret_cast<void**>(&pDesktop));
        if (FAILED(hr)) {
            continue;
        }

        HSTRING hName = nullptr;
        hr = pDesktop->GetName(&hName);
        if (SUCCEEDED(hr)) {
            const wchar_t* buf = WindowsGetStringRawBuffer(hName, nullptr);
            if (buf && name == buf) {
                *ppDesktop = pDesktop;
                WindowsDeleteString(hName);
                pArray->Release();
                return S_OK;
            }
            WindowsDeleteString(hName);
        }
        pDesktop->Release();
    }

    pArray->Release();
    return S_OK;  // 見つからなかった場合も正常終了（*ppDesktop == nullptr）
}

HRESULT CreateNamedDesktop(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name,
    IVirtualDesktop** ppDesktop)
{
    HRESULT hr = pManager->CreateDesktop(ppDesktop);
    if (FAILED(hr)) {
        fprintf(stderr, "CreateDesktop 失敗: 0x%08lX\n", hr);
        return hr;
    }

    HSTRING hName = nullptr;
    hr = WindowsCreateString(name.c_str(), static_cast<UINT32>(name.length()), &hName);
    if (FAILED(hr)) {
        fprintf(stderr, "WindowsCreateString 失敗: 0x%08lX\n", hr);
        (*ppDesktop)->Release();
        *ppDesktop = nullptr;
        return hr;
    }

    hr = pManager->SetDesktopName(*ppDesktop, hName);
    WindowsDeleteString(hName);

    if (FAILED(hr)) {
        fprintf(stderr, "SetDesktopName 失敗: 0x%08lX\n", hr);
        (*ppDesktop)->Release();
        *ppDesktop = nullptr;
        return hr;
    }

    return S_OK;
}

HRESULT SwitchToDesktop(
    IVirtualDesktopManagerInternal* pManager,
    IVirtualDesktop* pDesktop)
{
    HRESULT hr = pManager->SwitchDesktop(pDesktop);
    if (FAILED(hr)) {
        fprintf(stderr, "SwitchDesktop 失敗: 0x%08lX\n", hr);
    }
    return hr;
}

HRESULT RemoveDesktop(
    IVirtualDesktopManagerInternal* pManager,
    IVirtualDesktop* pDesktop)
{
    IObjectArray* pArray = nullptr;
    HRESULT hr = pManager->GetDesktops(&pArray);
    if (FAILED(hr)) {
        return hr;
    }

    UINT count = 0;
    pArray->GetCount(&count);

    // 削除対象が 0 番目の場合は 1 番目を、それ以外は 0 番目を fallback とする
    GUID idTarget = {};
    pDesktop->GetId(&idTarget);

    IVirtualDesktop* pFallback = nullptr;
    for (UINT i = 0; i < count; i++) {
        IVirtualDesktop* pCandidate = nullptr;
        hr = pArray->GetAt(i, __uuidof(IVirtualDesktop), reinterpret_cast<void**>(&pCandidate));
        if (FAILED(hr)) {
            continue;
        }

        GUID idCandidate = {};
        pCandidate->GetId(&idCandidate);

        if (!IsEqualGUID(idTarget, idCandidate)) {
            pFallback = pCandidate;
            break;
        }
        pCandidate->Release();
    }
    pArray->Release();

    if (!pFallback) {
        // デスクトップが 1 つしかない場合は削除不可
        fprintf(stderr, "削除できるデスクトップが他にない\n");
        return E_FAIL;
    }

    hr = pManager->RemoveDesktop(pDesktop, pFallback);
    pFallback->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "RemoveDesktop 失敗: 0x%08lX\n", hr);
    }
    return hr;
}

// EnumWindows コールバックのコンテキスト
//
// STA 再入リスクを避けるため、コールバック内で COM 呼び出しをしない。
// 対象ウィンドウの収集と COM 操作（PinView）を 2 段階に分けて処理する。
struct CollectContext {
    const std::vector<std::wstring>* pin_apps;
    // PID → exe ベース名キャッシュ（空文字列 = 取得失敗または対象外）
    std::unordered_map<DWORD, std::wstring> pid_cache;
    // 照合済みウィンドウ（Phase 2 で COM 操作を行う）
    std::vector<std::pair<HWND, std::wstring>> matched;
};

// EnumWindows コールバック（Phase 1：ウィンドウ収集のみ、COM 呼び出しなし）
static BOOL CALLBACK CollectWindowsProc(HWND hwnd, LPARAM lParam)
{
    // 不可視ウィンドウとツールウィンドウはスキップする
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    DWORD exStyle = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_EXSTYLE));
    if (exStyle & WS_EX_TOOLWINDOW) {
        return TRUE;
    }

    auto* ctx = reinterpret_cast<CollectContext*>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return TRUE;
    }

    // PID キャッシュから exe ベース名を取得（同一 PID は OS 問い合わせを1回に抑える）
    auto [it, inserted] = ctx->pid_cache.try_emplace(pid);
    if (inserted) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            wchar_t exePath[MAX_PATH] = {};
            DWORD len = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, exePath, &len)) {
                const wchar_t* p = wcsrchr(exePath, L'\\');
                it->second = p ? p + 1 : exePath;
            }
            CloseHandle(hProc);
        }
    }

    const std::wstring& exeBasename = it->second;
    if (exeBasename.empty()) {
        return TRUE;
    }

    // pin_apps リストと大文字小文字不問で照合
    bool matched = std::any_of(ctx->pin_apps->begin(), ctx->pin_apps->end(),
        [&](const auto& t) { return _wcsicmp(exeBasename.c_str(), t.c_str()) == 0; });
    if (matched) {
        ctx->matched.emplace_back(hwnd, exeBasename);
    }
    return TRUE;
}

HRESULT PinAppWindows(const std::vector<std::wstring>& pin_apps)
{
    if (pin_apps.empty()) {
        return S_OK;
    }

    IServiceProvider* pServiceProvider = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_ImmersiveShell, nullptr, CLSCTX_ALL,
        __uuidof(IServiceProvider), reinterpret_cast<void**>(&pServiceProvider));
    if (FAILED(hr)) {
        fprintf(stderr, "PinAppWindows: ImmersiveShell の取得失敗: 0x%08lX\n", hr);
        return hr;
    }

    IApplicationViewCollection* pViewCollection = nullptr;
    hr = pServiceProvider->QueryService(
        CLSID_ApplicationViewCollection,
        __uuidof(IApplicationViewCollection),
        reinterpret_cast<void**>(&pViewCollection));
    if (FAILED(hr)) {
        fprintf(stderr, "PinAppWindows: IApplicationViewCollection の取得失敗: 0x%08lX\n", hr);
        pServiceProvider->Release();
        return hr;
    }

    IVirtualDesktopPinnedApps* pPinnedApps = nullptr;
    hr = pServiceProvider->QueryService(
        CLSID_VirtualDesktopPinnedApps,
        __uuidof(IVirtualDesktopPinnedApps),
        reinterpret_cast<void**>(&pPinnedApps));
    pServiceProvider->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "PinAppWindows: IVirtualDesktopPinnedApps の取得失敗: 0x%08lX\n", hr);
        pViewCollection->Release();
        return hr;
    }

    // Phase 1: EnumWindows でウィンドウを収集（COM 呼び出しなし）
    CollectContext ctx = { &pin_apps };
    if (!EnumWindows(CollectWindowsProc, reinterpret_cast<LPARAM>(&ctx))) {
        fprintf(stderr, "EnumWindows 失敗: 0x%08lX\n", GetLastError());
    }

    // Phase 2: 収集したウィンドウに対して COM 操作（PinView）を実行
    for (const auto& [hwnd, exeBasename] : ctx.matched) {
        IApplicationView* pView = nullptr;
        HRESULT hr2 = pViewCollection->GetViewForHwnd(hwnd, &pView);
        if (FAILED(hr2) || !pView) {
            continue;
        }

        // 既にピン留め済みの場合はスキップ
        BOOL pinned = FALSE;
        hr2 = pPinnedApps->IsViewPinned(pView, &pinned);
        if (SUCCEEDED(hr2) && !pinned) {
            hr2 = pPinnedApps->PinView(pView);
            if (FAILED(hr2)) {
                fprintf(stderr, "PinView 失敗 (%ls): 0x%08lX\n", exeBasename.c_str(), hr2);
            }
        }
        pView->Release();
    }

    pPinnedApps->Release();
    pViewCollection->Release();
    return S_OK;
}
