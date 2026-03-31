// vim: set ft=cpp fenc=utf-8 ff=unix sw=4 ts=4 et :
#include "config.hpp"

#include <windows.h>
#include <cstdio>
#include <filesystem>

#define TOML_EXCEPTIONS 0
#include <toml.hpp>

namespace fs = std::filesystem;

// exe のあるディレクトリパスを取得する
//
// GetModuleFileNameW が MAX_PATH を返した場合はパスが切り詰められているため、
// カレントディレクトリにフォールバックする。
static fs::path GetExeDir()
{
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return fs::current_path();
    }
    return fs::path(buf).parent_path();
}

static std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) {
        return {};
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring wide(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), len);
    return wide;
}

// TOML テーブルから文字列配列を読み取り wstring ベクタに変換する
static std::vector<std::wstring> ReadStringArray(const toml::table& tbl, const char* key)
{
    std::vector<std::wstring> result;
    if (auto arr = tbl[key].as_array()) {
        for (auto& elem : *arr) {
            if (auto str = elem.as_string()) {
                auto wide = Utf8ToWide(str->get());
                if (!wide.empty()) {
                    result.push_back(std::move(wide));
                }
            }
        }
    }
    return result;
}

// TOML ファイルを読み込んで Config にマージする
//
// parse_file を先に試行し、失敗した場合は fs::exists でファイル不在かパースエラーかを判別する。
// ファイルが存在しなければ何もせずスキップし、存在すれば stderr にパースエラーを出力する。
static void MergeFromFile(const fs::path& path, Config& cfg)
{
    auto result = toml::parse_file(path.wstring());
    if (!result) {
        if (!fs::exists(path)) {
            return;
        }
        fprintf(stderr, "設定ファイルのパースに失敗しました: %ls\n  %s\n",
                path.c_str(), result.error().description().data());
        return;
    }

    const auto& tbl = result.table();

    if (tbl.contains("pre_exec"))   cfg.pre_exec   = ReadStringArray(tbl, "pre_exec");
    if (tbl.contains("post_exec"))  cfg.post_exec  = ReadStringArray(tbl, "post_exec");
    if (tbl.contains("close_exec")) cfg.close_exec = ReadStringArray(tbl, "close_exec");
}

Config LoadConfig()
{
    Config cfg;
    fs::path dir = GetExeDir();

    MergeFromFile(dir / L"vdsktop.toml", cfg);
    MergeFromFile(dir / L"vdsktop.local.toml", cfg);

    return cfg;
}

void ExecCommands(const std::vector<std::wstring>& commands)
{
    for (const auto& cmd : commands) {
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        // CreateProcessW は lpCommandLine を書き換える可能性があるためコピーする
        std::wstring cmdLine = cmd;

        BOOL ok = CreateProcessW(
            nullptr, cmdLine.data(),
            nullptr, nullptr, FALSE,
            0, nullptr, nullptr,
            &si, &pi);

        if (!ok) {
            fprintf(stderr, "コマンドの実行に失敗しました: %ls (error=%lu)\n",
                    cmd.c_str(), GetLastError());
            continue;
        }

        // スレッドハンドルは不要なので即座に閉じる
        CloseHandle(pi.hThread);

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != 0) {
            fprintf(stderr, "コマンドが非ゼロで終了しました: %ls (exit=%lu)\n",
                    cmd.c_str(), exitCode);
        }

        CloseHandle(pi.hProcess);
    }
}
