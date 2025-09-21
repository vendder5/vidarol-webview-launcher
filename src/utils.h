#pragma once
#include <string>

namespace Utils
{
    std::wstring getExecutableDir();
    std::wstring getLocalAppDataPath();
    std::wstring Utf8ToWide(const std::string& utf8_string);
    std::string WideToUtf8(const std::wstring& wide_string);
    std::string LoadResourceAsString(int resourceID);
    std::string EscapeJavaScriptString(std::string str);
}