// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :

// C++/WinRT ヘッダは windows.h より先にインクルードする
#include <winrt/base.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

#include <windows.h>
#include <shobjidl_core.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>

#include <cstdio>
#include <string>

#include "toast.hpp"

// Toast 通知の識別子
static const wchar_t* APP_AUMID = L"com.vdskswtch";

// スタートメニューにショートカットを作成する
//
// デスクトップアプリが Toast 通知を表示するには AUMID 付きの .lnk が
// スタートメニューに存在する必要がある（Windows の制約）。
// ショートカットが既に存在する場合は何もしない。
static void ensureShortcut()
{
    wchar_t appData[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
        return;
    }

    std::wstring linkPath = std::wstring(appData)
        + L"\\Microsoft\\Windows\\Start Menu\\Programs\\vdskswtch.lnk";

    if (GetFileAttributesW(linkPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }

    wchar_t exePath[MAX_PATH] = {};
    DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exeLen == 0 || exeLen == MAX_PATH) {
        return;
    }

    winrt::com_ptr<IShellLinkW> psl;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(psl.put())))) {
        return;
    }

    psl->SetPath(exePath);

    if (auto pps = psl.as<IPropertyStore>()) {
        PROPVARIANT pv;
        if (SUCCEEDED(InitPropVariantFromString(APP_AUMID, &pv))) {
            pps->SetValue(PKEY_AppUserModel_ID, pv);
            PropVariantClear(&pv);
        }
        pps->Commit();
    }

    if (auto ppf = psl.as<IPersistFile>()) {
        ppf->Save(linkPath.c_str(), TRUE);
    }
}

void InitToast()
{
    SetCurrentProcessExplicitAppUserModelID(APP_AUMID);
    ensureShortcut();
}

// XML の特殊文字をエスケープする
static std::wstring escapeXml(const std::wstring& s)
{
    std::wstring r;
    r.reserve(s.size() + 16);
    for (wchar_t c : s) {
        switch (c) {
        case L'&':  r += L"&amp;";  break;
        case L'<':  r += L"&lt;";   break;
        case L'>':  r += L"&gt;";   break;
        case L'"':  r += L"&quot;"; break;
        default:    r += c;
        }
    }
    return r;
}

void ShowToast(const wchar_t* message)
{
    try {
        std::wstring xml =
            L"<toast>"
            L"<visual><binding template=\"ToastGeneric\">"
            L"<text>" + escapeXml(message) + L"</text>"
            L"</binding></visual>"
            L"</toast>";

        winrt::Windows::Data::Xml::Dom::XmlDocument doc;
        doc.LoadXml(xml);

        auto notifier = winrt::Windows::UI::Notifications::ToastNotificationManager
            ::CreateToastNotifier(APP_AUMID);
        auto notification = winrt::Windows::UI::Notifications::ToastNotification(doc);

        notifier.Show(notification);
    }
    catch (const winrt::hresult_error& e) {
        fprintf(stderr, "Toast 通知の表示に失敗しました: 0x%08X\n",
                static_cast<unsigned int>(e.code()));
    }
}
