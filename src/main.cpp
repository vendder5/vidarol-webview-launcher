#include "main.h"
#include "config.h"
#include "resource.h"
#include <wil/com.h>
#include <dwmapi.h>
#include <string_view>
#include <filesystem>
#include <stdexcept>

#pragma comment(lib, "dwmapi.lib")

using namespace Microsoft::WRL;

enum class WindowMessage : UINT {
    DownloadComplete = WM_APP + 1,
};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    constexpr LPCWSTR CLASS_NAME = L"vidarol";
    constexpr LPCWSTR WINDOW_TITLE = L"VidaRol - Launcher";
    constexpr int WINDOW_WIDTH = 770;
    constexpr int WINDOW_HEIGHT = 570;

    HWND hExistingWnd = FindWindowW(CLASS_NAME, WINDOW_TITLE);
    if (hExistingWnd)
    {
        ShowWindow(hExistingWnd, SW_RESTORE);
        SetForegroundWindow(hExistingWnd);
        return 0;
    }


    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MAINICON));
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MAINICON));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wcex.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wcex);

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (screenWidth - WINDOW_WIDTH) / 2;
    const int posY = (screenHeight - WINDOW_HEIGHT) / 2;

    auto appState = std::make_shared<AppState>();

    HWND hWnd = CreateWindowExW(
        WS_EX_COMPOSITED,
        CLASS_NAME, WINDOW_TITLE,
        WS_POPUP, posX, posY, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, appState.get());

    if (!hWnd)
    {
        return FALSE;
    }

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [appState](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                return OnCreateEnvironmentCompleted(result, env, appState);
            }).Get());

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (appState->downloadThread.joinable()) {
        appState->downloadThread.detach();
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    AppState* appState = reinterpret_cast<AppState*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
        case WM_CREATE:
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            AppState* initialState = static_cast<AppState*>(pCreate->lpCreateParams);
            initialState->hWnd = hWnd;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(initialState));
            break;
        }
        case WM_NCHITTEST:
        {
            LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
            return (hit == HTCLIENT) ? HTCAPTION : hit;
        }
        case WM_SIZE:
        {
            if (appState && appState->webviewController)
            {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                appState->webviewController->put_Bounds(bounds);
            }
            break;
        }
        case static_cast<UINT>(WindowMessage::DownloadComplete):
        {
            if (appState && appState->webview)
            {
                bool success = static_cast<bool>(wParam);
                appState->webview->PostWebMessageAsString(success ? L"downloads-ready" : L"downloads-failed");
            }
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HRESULT OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* environment, std::shared_ptr<AppState> appState)
{
    if (FAILED(result)) return result;

    return environment->CreateCoreWebView2Controller(appState->hWnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [appState](HRESULT res, ICoreWebView2Controller* controller) -> HRESULT {
                return OnCreateControllerCompleted(res, controller, appState);
            }).Get());
}

HRESULT OnCreateControllerCompleted(HRESULT result, ICoreWebView2Controller* controller, std::shared_ptr<AppState> appState)
{
    if (FAILED(result)) return result;

    appState->webviewController = controller;
    appState->webviewController->get_CoreWebView2(&appState->webview);

    ComPtr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller->QueryInterface(__uuidof(ICoreWebView2Controller2), &controller2)))
    {
        COREWEBVIEW2_COLOR color = {};
        controller2->put_DefaultBackgroundColor(color);
    }

    RECT bounds;
    GetClientRect(appState->hWnd, &bounds);
    appState->webviewController->put_Bounds(bounds);

    EventRegistrationToken msgToken;
    appState->webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [appState](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                return OnWebMessageReceived(webview, args, appState);
            }).Get(), &msgToken);

    EventRegistrationToken navToken;
    appState->webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                std::string playerName = Config::loadPlayerName();
                if (!playerName.empty())
                {
                    std::string script = "document.getElementById('playerNameInput').value = '" + Utils::EscapeJavaScriptString(playerName) + "';";
                    sender->ExecuteScript(Utils::Utf8ToWide(script).c_str(), nullptr);
                }
                return S_OK;
            }).Get(), &navToken);
    
    StartBackgroundDownloads(appState);

    std::string htmlContent = Utils::LoadResourceAsString(IDR_HTML1);
    return appState->webview->NavigateToString(Utils::Utf8ToWide(htmlContent).c_str());
}

void StartBackgroundDownloads(std::shared_ptr<AppState> appState)
{
    appState->downloadThread = std::thread([appState]
    {
        std::filesystem::path destinationDir;
        try {
            destinationDir = Utils::getLocalAppDataPath();
            if (destinationDir.empty()) {
                throw std::runtime_error("Could not get Local AppData Path.");
            }
            destinationDir /= "Programs/WinUtils/x86/launcher/";
            std::filesystem::create_directories(destinationDir);
        }
        catch (const std::exception&)
        {
            if (appState->hWnd) {
                PostMessage(appState->hWnd, static_cast<UINT>(WindowMessage::DownloadComplete), FALSE, 0);
            }
            return;
        }

        const std::wstring sampUrl = AY_OBFUSCATE(L"https://update-launcher.vida-roleplay.es/samp.dll");
        const std::wstring bacUrl = AY_OBFUSCATE(L"https://update-launcher.vida-roleplay.es/bac.dll");

        const std::filesystem::path sampDestinationPath = destinationDir / std::string(AY_OBFUSCATE("samp.dll"));
        const std::filesystem::path bacDestinationPath = destinationDir / std::string(AY_OBFUSCATE("bac.dll"));
        
        const bool downloadsOk = Downloader::downloadFile(sampUrl, sampDestinationPath.wstring()) &&
                                 Downloader::downloadFile(bacUrl, bacDestinationPath.wstring());
        
        appState->downloadSucceeded = downloadsOk;
        appState->downloadsReady = true;

        if (appState->hWnd) {
            PostMessage(appState->hWnd, static_cast<UINT>(WindowMessage::DownloadComplete), static_cast<WPARAM>(downloadsOk), 0);
        }
    });
}

HRESULT OnWebMessageReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args, std::shared_ptr<AppState> appState)
{
    wil::unique_cotaskmem_string message;
    args->TryGetWebMessageAsString(&message);
    std::string narrowMessage = Utils::WideToUtf8(message.get());

    if (narrowMessage == "close-app")
    {
        PostMessage(appState->hWnd, WM_CLOSE, 0, 0);
    }
    else if (narrowMessage == "minimize-app")
    {
        ShowWindow(appState->hWnd, SW_MINIMIZE);
    }
    else if (narrowMessage.rfind("launch-game:", 0) == 0) 
    {
        HandleLaunchGameMessage(narrowMessage, appState.get());
    }
    return S_OK;
}

void HandleLaunchGameMessage(const std::string& narrowMessage, AppState* appState)
{
    if (!appState->downloadsReady.load())
    {
        MessageBoxW(appState->hWnd, L"Los archivos necesarios aún se están descargando. Por favor, espera un momento e inténtalo de nuevo.", L"Descargando", MB_ICONINFORMATION);
        return;
    }

    if (!appState->downloadSucceeded.load())
    {
        MessageBoxW(appState->hWnd, L"No se pudieron descargar los archivos necesarios.", L"Error de Descarga", MB_ICONERROR);
        return;
    }

    constexpr std::string_view prefix = "launch-game:";
    std::string_view data(narrowMessage.data() + prefix.length(), narrowMessage.length() - prefix.length());

    const size_t first_delim = data.find(':');
    const size_t second_delim = data.find(':', first_delim + 1);

    if (first_delim == std::string_view::npos || second_delim == std::string_view::npos)
    {
        MessageBoxW(appState->hWnd, L"Formato de datos de lanzamiento inválido.", L"Error", MB_ICONERROR);
        return;
    }

    std::string ip(data.substr(0, first_delim));
    std::string_view port_sv = data.substr(first_delim + 1, second_delim - (first_delim + 1));
    std::string name(data.substr(second_delim + 1));

    if (ip.empty() || port_sv.empty() || name.empty())
    {
        MessageBoxW(appState->hWnd, L"Los datos de lanzamiento están incompletos.", L"Error", MB_ICONERROR);
        return;
    }
    
    int port = 0;
    try
    {
        port = std::stoi(std::string(port_sv));
    }
    catch (const std::invalid_argument&)
    {
        MessageBoxW(appState->hWnd, L"Puerto Invalido.", L"Error", MB_ICONERROR);
        return;
    }
    catch (const std::out_of_range&)
    {
        MessageBoxW(appState->hWnd, L"Fuera de rango.", L"Error", MB_ICONERROR);
        return;
    }

    Config::savePlayerName(name);

    if (Injector::injectSamp(name, ip, port))
    {
        PostMessage(appState->hWnd, WM_CLOSE, 0, 0);
    }
    else
    {
        MessageBoxW(appState->hWnd, L"No se pudo inyectar en el proceso del juego. Asegúrate de que tu gta no esté en ejecución o que tengas los permisos necesarios.", L"Error de Inyección", MB_ICONERROR);
    }
}