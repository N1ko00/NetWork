#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog, IFileSaveDialog
#include <wrl/client.h> // Microsoft::WRL::ComPtr
#include <vector>
#include "fileopendialog.h"

#pragma comment(lib, "Ole32.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    struct ComInitScope
    {
        HRESULT hr{};
        ComInitScope()
        {
            // UI系は STA 推奨
            hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        }
        ~ComInitScope()
        {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
        bool ok() const { return SUCCEEDED(hr); }
    };

    void ApplyInitialDir(IFileDialog* dlg, const std::filesystem::path& initialDir)
    {
        if (initialDir.empty()) return;

        ComPtr<IShellItem> folderItem;
        const std::wstring w = initialDir.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(w.c_str(), nullptr, IID_PPV_ARGS(&folderItem))))
        {
            dlg->SetFolder(folderItem.Get());
        }
    }

    void ApplyTitle(IFileDialog* dlg, const std::wstring& title)
    {
        if (!title.empty()) dlg->SetTitle(title.c_str());
    }

    void ApplyFilters(IFileDialog* dlg, const std::vector<FileTypeFilter>& filters)
    {
        if (filters.empty()) return;

        std::vector<COMDLG_FILTERSPEC> specs;
        specs.reserve(filters.size());
        for (auto& f : filters)
        {
            COMDLG_FILTERSPEC s{};
            s.pszName = f.name.c_str();
            s.pszSpec = f.spec.c_str();
            specs.push_back(s);
        }
        dlg->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        dlg->SetFileTypeIndex(1);
    }

    std::optional<std::filesystem::path> GetSingleResult(IFileDialog* dlg)
    {
        ComPtr<IShellItem> item;
        if (FAILED(dlg->GetResult(&item))) return std::nullopt;

        PWSTR psz = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &psz)) || !psz) return std::nullopt;

        std::filesystem::path p = psz;
        CoTaskMemFree(psz);
        return p;
    }

    std::vector<std::filesystem::path> GetMultiResults(IFileOpenDialog* dlg)
    {
        std::vector<std::filesystem::path> out;

        ComPtr<IShellItemArray> items;
        if (FAILED(dlg->GetResults(&items))) return out;

        DWORD count = 0;
        if (FAILED(items->GetCount(&count))) return out;

        out.reserve(count);
        for (DWORD i = 0; i < count; ++i)
        {
            ComPtr<IShellItem> item;
            if (FAILED(items->GetItemAt(i, &item))) continue;

            PWSTR psz = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &psz)) && psz)
            {
                out.emplace_back(psz);
                CoTaskMemFree(psz);
            }
        }
        return out;
    }
}

std::optional<std::filesystem::path> FileDialog::OpenFile(
    const std::wstring& title,
    const std::filesystem::path& initialDir,
    const std::vector<FileTypeFilter>& filters,
    bool mustExist)
{
    ComInitScope com;
    if (!com.ok()) return std::nullopt;

    ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
        return std::nullopt;

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM;
    if (mustExist) opts |= FOS_FILEMUSTEXIST;
    dlg->SetOptions(opts);

    ApplyTitle(dlg.Get(), title);
    ApplyInitialDir(dlg.Get(), initialDir);
    ApplyFilters(dlg.Get(), filters);

    if (FAILED(dlg->Show(nullptr)))
        return std::nullopt;

    return GetSingleResult(dlg.Get());
}

std::vector<std::filesystem::path> FileDialog::OpenFiles(
    const std::wstring& title,
    const std::filesystem::path& initialDir,
    const std::vector<FileTypeFilter>& filters)
{
    ComInitScope com;
    if (!com.ok()) return {};

    ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
        return {};

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);

    ApplyTitle(dlg.Get(), title);
    ApplyInitialDir(dlg.Get(), initialDir);
    ApplyFilters(dlg.Get(), filters);

    if (FAILED(dlg->Show(nullptr)))
        return {};

    return GetMultiResults(dlg.Get());
}

std::optional<std::filesystem::path> FileDialog::SaveFile(
    const std::wstring& title,
    const std::filesystem::path& initialDir,
    const std::vector<FileTypeFilter>& filters,
    const std::wstring& defaultExt)
{
    ComInitScope com;
    if (!com.ok()) return std::nullopt;

    ComPtr<IFileSaveDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
        return std::nullopt;

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM;
    dlg->SetOptions(opts);

    ApplyTitle(dlg.Get(), title);
    ApplyInitialDir(dlg.Get(), initialDir);
    ApplyFilters(dlg.Get(), filters);

    if (!defaultExt.empty())
        dlg->SetDefaultExtension(defaultExt.c_str());

    if (FAILED(dlg->Show(nullptr)))
        return std::nullopt;

    return GetSingleResult(dlg.Get());
}

std::optional<std::filesystem::path> FileDialog::PickFolder(
    const std::wstring& title,
    const std::filesystem::path& initialDir)
{
    ComInitScope com;
    if (!com.ok()) return std::nullopt;

    ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
        return std::nullopt;

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    opts |= FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS;
    dlg->SetOptions(opts);

    ApplyTitle(dlg.Get(), title);
    ApplyInitialDir(dlg.Get(), initialDir);

    if (FAILED(dlg->Show(nullptr)))
        return std::nullopt;

    return GetSingleResult(dlg.Get());
}

// ---- UTF-8 / UTF-16 変換 ----
std::string WideToUtf8(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

//static std::string PathToUtf8String(const std::filesystem::path& p)
//{
//    // Windowsでは path 内部が UTF-16（wstringベース）なので、それを UTF-8 に変換
//    return WideToUtf8(p.filename().wstring());
//}
