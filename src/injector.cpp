#include "injector.h"
#include "utils.h"
#include <windows.h>
#include <vector>
#include <tlhelp32.h>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

namespace {

const wchar_t* LAUNCHER_AUTH_TOKEN_NAME = AY_OBFUSCATE(L"ac_token");
const wchar_t* LAUNCHER_AUTH_TOKEN_VALUE = AY_OBFUSCATE(L"sexo-422-1337");

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const wchar_t* name, const wchar_t* value) : name_(name)
    {
        SetEnvironmentVariableW(name_, value);
    }
    ~ScopedEnvironmentVariable()
    {
        SetEnvironmentVariableW(name_, nullptr);
    }
private:
    const wchar_t* name_;
};

bool apcInject(HANDLE hProcess, HANDLE hThread, const std::wstring& dllPath)
{
    FARPROC pLoadLibraryW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!pLoadLibraryW)
        return false;

    size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID pRemoteDllPath = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteDllPath)
        return false;

    if (!WriteProcessMemory(hProcess, pRemoteDllPath, dllPath.c_str(), dllPathSize, NULL))
    {
        VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
        return false;
    }

    if (QueueUserAPC(reinterpret_cast<PAPCFUNC>(pLoadLibraryW), hThread, reinterpret_cast<ULONG_PTR>(pRemoteDllPath)) == 0)
    {
        VirtualFreeEx(hProcess, pRemoteDllPath, 0, MEM_RELEASE);
        return false;
    }

    return true;
}

}

bool Injector::injectSamp(const std::string& name, const std::string& ip, int port)
{
    std::wstring name_w = Utils::Utf8ToWide(name);
    std::wstring ip_w = Utils::Utf8ToWide(ip);
    
    fs::path gamePath = Utils::getExecutableDir();
    if (gamePath.empty())
    {
        MessageBoxW(NULL, L"could not determine the application path.", L"Error", MB_ICONERROR);
        return false;
    }

    fs::path exePath = gamePath / "gta_sa.exe";
    
    std::wstring commandLineStr = L"\"" + exePath.wstring() + L"\" -c -n " + name_w +
                                    L" -h " + ip_w + L" -p " + std::to_wstring(port) +
                                    L" -z vrAs@!2036_";
    std::vector<wchar_t> commandLine(commandLineStr.begin(), commandLineStr.end());
    commandLine.push_back(0);

    PROCESS_INFORMATION processInfo = {};
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    ScopedEnvironmentVariable authFlag(LAUNCHER_AUTH_TOKEN_NAME, LAUNCHER_AUTH_TOKEN_VALUE);

    if (!CreateProcessW(NULL, commandLine.data(), nullptr, nullptr, false, CREATE_SUSPENDED, nullptr, gamePath.c_str(), &startupInfo, &processInfo))
    {
        DWORD errorCode = GetLastError();
        wchar_t errorMsg[1024];
        swprintf_s(errorMsg, L"Error: %lu\n\nmake sure the launcher is in the same folder as gta_sa.exe.", errorCode);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return false;
    }
    
    fs::path appDataPath = Utils::getLocalAppDataPath();
    if (appDataPath.empty())
    {
        MessageBoxW(NULL, L"Could not get the AppData path.", L"Error", MB_ICONERROR);
        TerminateProcess(processInfo.hProcess, 0);
        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);
        return false;
    }

    fs::path dllDir = appDataPath / std::string(AY_OBFUSCATE("Programs/WinUtils/x86/launcher"));

    const std::vector<std::wstring> dllsToInject =
    {
        std::wstring(AY_OBFUSCATE(L"samp.dll")),
        std::wstring(AY_OBFUSCATE(L"bac.dll"))
    };
    
    for (const auto& dllName : dllsToInject)
    {
        fs::path dllFullPath = dllDir / dllName;
        if (!apcInject(processInfo.hProcess, processInfo.hThread, dllFullPath.wstring()))
        {
            wchar_t errorMsg[512];
            swprintf_s(errorMsg, L"injection failed: %s\n\nfile may be corrupt or blocked.", dllName.c_str());
            MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
            TerminateProcess(processInfo.hProcess, 0);
            CloseHandle(processInfo.hProcess);
            CloseHandle(processInfo.hThread);
            return false;
        }
    }

    ResumeThread(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return true;
}