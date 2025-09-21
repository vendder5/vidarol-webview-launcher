#include "utils.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

namespace Utils
{
    std::wstring getExecutableDir()
    {
        wchar_t path_buffer[MAX_PATH];
        GetModuleFileNameW(NULL, path_buffer, MAX_PATH);
        return fs::path(path_buffer).parent_path().wstring();
    }

    std::wstring getLocalAppDataPath()
    {
        PWSTR path_ptr = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path_ptr);
        std::unique_ptr<WCHAR, decltype(&CoTaskMemFree)> smart_path_ptr(path_ptr, &CoTaskMemFree);
        if (SUCCEEDED(hr) && smart_path_ptr)
        {
            return std::wstring(smart_path_ptr.get());
        }
        return {};
    }

    std::wstring Utf8ToWide(const std::string& utf8_string)
    {
        if (utf8_string.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_string.data(), static_cast<int>(utf8_string.size()), NULL, 0);
        std::wstring wide_string(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8_string.data(), static_cast<int>(utf8_string.size()), wide_string.data(), size_needed);
        return wide_string;
    }

    std::string WideToUtf8(const std::wstring& wide_string)
    {
        if (wide_string.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), static_cast<int>(wide_string.size()), NULL, 0, NULL, NULL);
        std::string utf8_string(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), static_cast<int>(wide_string.size()), utf8_string.data(), size_needed, NULL, NULL);
        return utf8_string;
    }

    std::string LoadResourceAsString(int resourceID)
    {
        HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(resourceID), L"TEXTFILE");
        if (!hResource)
            return "";

        HGLOBAL hLoadedResource = LoadResource(nullptr, hResource);
        if (!hLoadedResource)
            return "";

        LPVOID pLockedResource = LockResource(hLoadedResource);
        if (!pLockedResource)
            return "";

        DWORD dwResourceSize = SizeofResource(nullptr, hResource);
        if (dwResourceSize == 0)
            return "";

        return std::string(static_cast<const char*>(pLockedResource), dwResourceSize);
    }

    std::string EscapeJavaScriptString(std::string str) {
        size_t pos = 0;
        while ((pos = str.find('\'', pos)) != std::string::npos) {
            str.replace(pos, 1, "\\'");
            pos += 2;
        }
        return str;
    }
}