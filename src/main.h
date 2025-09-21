#pragma once

#include <windows.h>
#include <wrl.h>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include "WebView2.h"

#include "obfuscate.h"
#include "injector.h"
#include "downloader.h"
#include "utils.h"

using Microsoft::WRL::ComPtr;

struct AppState
{
    HWND hWnd = nullptr;
    ComPtr<ICoreWebView2Controller> webviewController;
    ComPtr<ICoreWebView2> webview;
    std::thread downloadThread;
    std::atomic_bool downloadsReady = false;
    std::atomic_bool downloadSucceeded = false;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
HRESULT OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* environment, std::shared_ptr<AppState> appState);
HRESULT OnCreateControllerCompleted(HRESULT result, ICoreWebView2Controller* controller, std::shared_ptr<AppState> appState);
HRESULT OnWebMessageReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args, std::shared_ptr<AppState> appState);
void StartBackgroundDownloads(std::shared_ptr<AppState> appState);
void HandleLaunchGameMessage(const std::string& narrowMessage, AppState* appState);
bool LaunchUpdater();
