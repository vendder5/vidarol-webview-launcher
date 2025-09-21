#include "downloader.h"
#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>
#include <filesystem>
#include <memory>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace Downloader
{
    bool downloadFile(const std::wstring& url, const std::wstring& destinationPath)
    {
        std::filesystem::path destPathFs(destinationPath);
        std::filesystem::path dirPath = destPathFs.parent_path();

        if (!std::filesystem::exists(dirPath))
        {
            if (!std::filesystem::create_directories(dirPath))
                return false;
        }

        URL_COMPONENTSW urlComp = {};
        wchar_t hostName[256];
        wchar_t urlPath[2048];
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = 2048;

        if (!WinHttpCrackUrl(url.c_str(), url.length(), 0, &urlComp))
            return false;

        auto internetHandleDeleter = [](HINTERNET h) { if (h) WinHttpCloseHandle(h); };
        using unique_hinternet = std::unique_ptr<void, decltype(internetHandleDeleter)>;

        unique_hinternet hSession(WinHttpOpen(L"Putojajaja/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0), internetHandleDeleter);
        if (!hSession)
            return false;

        unique_hinternet hConnect(WinHttpConnect(hSession.get(), hostName, urlComp.nPort, 0), internetHandleDeleter);
        if (!hConnect)
            return false;
        
        DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        unique_hinternet hRequest(WinHttpOpenRequest(hConnect.get(), L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags), internetHandleDeleter);
        if (!hRequest)
            return false;

        if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            return false;

        if (!WinHttpReceiveResponse(hRequest.get(), NULL))
            return false;

        auto fileHandleDeleter = [](HANDLE h) { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); };
        using unique_hfile = std::unique_ptr<void, decltype(fileHandleDeleter)>;
        
        unique_hfile hFile(CreateFileW(destinationPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL), fileHandleDeleter);
        if (hFile.get() == INVALID_HANDLE_VALUE)
            return false;

        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        std::vector<BYTE> buffer;
        do
        {
            if (!WinHttpQueryDataAvailable(hRequest.get(), &dwSize)) return false;
            if (dwSize > 0)
            {
                buffer.resize(dwSize);
                if (WinHttpReadData(hRequest.get(), buffer.data(), dwSize, &dwDownloaded))
                {
                    DWORD dwWritten = 0;
                    if (!WriteFile(hFile.get(), buffer.data(), dwDownloaded, &dwWritten, NULL)) return false;
                }
                else
                {
                    return false;
                }
            }
        } while (dwSize > 0);

        return true;
    }
}