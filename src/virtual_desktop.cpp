// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#include "virtual_desktop.hpp"
#include <cstdio>

HRESULT InitVirtualDesktopManager(IVirtualDesktopManagerInternal** ppManager)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // S_FALSE は既に初期化済みを表す正常状態
    if (FAILED(hr)) {
        fprintf(stderr, "CoInitializeEx 失敗: 0x%08lX\n", hr);
        return hr;
    }

    IServiceProvider* pServiceProvider = nullptr;
    hr = CoCreateInstance(
        CLSID_ImmersiveShell, nullptr, CLSCTX_LOCAL_SERVER,
        __uuidof(IServiceProvider), reinterpret_cast<void**>(&pServiceProvider));
    if (FAILED(hr)) {
        fprintf(stderr, "ImmersiveShell の取得失敗: 0x%08lX\n", hr);
        CoUninitialize();
        return hr;
    }

    hr = pServiceProvider->QueryService(
        CLSID_VirtualDesktopManagerInternal,
        __uuidof(IVirtualDesktopManagerInternal),
        reinterpret_cast<void**>(ppManager));
    pServiceProvider->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "IVirtualDesktopManagerInternal の取得失敗: 0x%08lX\n", hr);
        CoUninitialize();
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
            UINT32 len = 0;
            const wchar_t* buf = WindowsGetStringRawBuffer(hName, &len);
            if (buf && name == buf) {
                // 名前が一致したのでポインタを返す（Release は呼び出し元の責任）
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

HRESULT RemoveDesktopByName(
    IVirtualDesktopManagerInternal* pManager,
    const std::wstring& name)
{
    // 削除対象のデスクトップを検索
    IVirtualDesktop* pTarget = nullptr;
    HRESULT hr = FindDesktopByName(pManager, name, &pTarget);
    if (FAILED(hr)) {
        return hr;
    }
    if (!pTarget) {
        fprintf(stderr, "デスクトップ \"%ls\" が見つからない\n", name.c_str());
        return E_FAIL;
    }

    // fallback（最初のデスクトップ）を取得
    IObjectArray* pArray = nullptr;
    hr = pManager->GetDesktops(&pArray);
    if (FAILED(hr)) {
        pTarget->Release();
        return hr;
    }

    UINT count = 0;
    pArray->GetCount(&count);

    // 最初の（インデックス 0 の）デスクトップを fallback とする。
    // 削除対象が 0 番目の場合は 1 番目を fallback とする。
    IVirtualDesktop* pFallback = nullptr;
    for (UINT i = 0; i < count; i++) {
        IVirtualDesktop* pCandidate = nullptr;
        hr = pArray->GetAt(i, __uuidof(IVirtualDesktop), reinterpret_cast<void**>(&pCandidate));
        if (FAILED(hr)) {
            continue;
        }

        GUID idTarget = {}, idCandidate = {};
        pTarget->GetId(&idTarget);
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
        pTarget->Release();
        return E_FAIL;
    }

    hr = pManager->RemoveDesktop(pTarget, pFallback);
    pTarget->Release();
    pFallback->Release();

    if (FAILED(hr)) {
        fprintf(stderr, "RemoveDesktop 失敗: 0x%08lX\n", hr);
    }
    return hr;
}
