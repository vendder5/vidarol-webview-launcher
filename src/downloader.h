#pragma once

#include <string>

namespace Downloader
{
    bool downloadFile(const std::wstring& url, const std::wstring& destinationPath);
}