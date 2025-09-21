#include "config.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>

static std::wstring GetConfigFilePath()
{
    PWSTR path = NULL;

    HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path);
    if (FAILED(hr))
    {
        CoTaskMemFree(path);
        return L"";
    }

    std::wstring configDir = std::wstring(path) + L"\\VidaRol"; // this shit generate "VidaRol" folder at AppData/Roaming
    CoTaskMemFree(path);

    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);

    if (ec)
        return L"";
    
    return configDir + L"\\launcher.cfg";
}

void Config::savePlayerName(const std::string& name)
{
    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty())
        return;

    std::ofstream configFile(filePath);
    if (configFile.is_open())
    {
        configFile << name;
        configFile.close();
    }
}

std::string Config::loadPlayerName()
{
    std::wstring filePath = GetConfigFilePath();
    if (filePath.empty())
        return "";

    std::ifstream configFile(filePath);
    if (configFile.is_open())
    {
        std::string name;
        std::getline(configFile, name);
        configFile.close();
        return name;
    }
    return "";
}
