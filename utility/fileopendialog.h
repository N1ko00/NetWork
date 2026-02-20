#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct FileTypeFilter
{
    // 例: L"Image Files"
    std::wstring name;
    // 例: L"*.png;*.jpg;*.jpeg;*.tga"
    std::wstring spec;
};

class FileDialog
{
public:
    // 単一ファイル選択
    static std::optional<std::filesystem::path> OpenFile(
        const std::wstring& title = L"Open File",
        const std::filesystem::path& initialDir = {},
        const std::vector<FileTypeFilter>& filters = {},
        bool mustExist = true
    );

    // 複数ファイル選択
    static std::vector<std::filesystem::path> OpenFiles(
        const std::wstring& title = L"Open Files",
        const std::filesystem::path& initialDir = {},
        const std::vector<FileTypeFilter>& filters = {}
    );

    // 保存先選択
    static std::optional<std::filesystem::path> SaveFile(
        const std::wstring& title = L"Save File",
        const std::filesystem::path& initialDir = {},
        const std::vector<FileTypeFilter>& filters = {},
        const std::wstring& defaultExt = L""
    );

    // フォルダ選択
    static std::optional<std::filesystem::path> PickFolder(
        const std::wstring& title = L"Select Folder",
        const std::filesystem::path& initialDir = {}
    );
};

// UTF-8文字列が欲しい場合の補助（std::string）
static std::string WideToUtf8(const std::wstring& ws);
static std::wstring Utf8ToWide(const std::string& s);

// path -> UTF-8 string（ログ/保存用）
// static std::string PathToUtf8String(const std::filesystem::path& p);
