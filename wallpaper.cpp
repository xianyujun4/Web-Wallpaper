// Web-Wallpaper 桌面主程序（半透明可交互版）
// 编译命令：
// g++ -o wallpaper.exe wallpaper.cpp -I"./sdk/WebView2/build/native/include" -L"./sdk/WebView2/build/native/x64" "./sdk/WebView2/build/native/x64/WebView2Loader.dll.lib" -lshell32 -lole32 -luser32 -lgdi32 -lws2_32 -mwindows -std=c++17

#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <WebView2.h>
#include <cstdio>
#include <cstdarg>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define WM_TRAY         (WM_USER + 1)
#define IDM_RELOAD      1001
#define IDM_EXIT        1002
#define MUTEX_NAME      L"WebWallpaper_SingleInstance"
#define CLASS_NAME      L"WebWallpaperWindow"

HWND                              g_hWnd       = NULL;
NOTIFYICONDATAW                   g_nid        = {};
ICoreWebView2Controller*          g_controller = nullptr;
ICoreWebView2*                    g_webview    = nullptr;
std::wstring                      g_exeDir;

void Log(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s\n", buf);
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.rfind(L'\\');
    return (pos != std::wstring::npos) ? p.substr(0, pos) : p;
}

std::wstring GetLoadUrl() {
    return L"http://localhost:5173";
}

// 托盘图标
void AddTrayIcon(HWND hWnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    wcscpy_s(g_nid.szTip, L"Web-Wallpaper");
    std::wstring iconPath = g_exeDir + L"\\icon.ico";
    g_nid.hIcon = (HICON)LoadImageW(NULL, iconPath.c_str(), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

void ShowTrayMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"模式：开发 (localhost:5173)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_RELOAD, L"重新加载");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

// WebView2 消息回调
struct WebMessageHandler : ICoreWebView2WebMessageReceivedEventHandler {
    ULONG refCount;
    WebMessageHandler() : refCount(1) {}
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        LPWSTR msg = nullptr;
        args->TryGetWebMessageAsString(&msg);
        if (!msg) return S_OK;
        std::wstring message(msg);
        CoTaskMemFree(msg);
        Log("Received message: %S", message.c_str());
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override { if (--refCount == 0) { delete this; return 0; } return refCount; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
        AddRef();
        return S_OK;
    }
};

struct CtrlCompletedHandler : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    HWND hWnd;
    ULONG refCount;
    CtrlCompletedHandler(HWND h) : hWnd(h), refCount(1) {}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        if (FAILED(result) || !controller) return result;

        g_controller = controller;
        g_controller->AddRef();
        g_controller->get_CoreWebView2(&g_webview);

        // 配置 WebView2
        ICoreWebView2Settings* settings = nullptr;
        g_webview->get_Settings(&settings);
        if (settings) {
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_IsWebMessageEnabled(TRUE);
            settings->put_AreDevToolsEnabled(TRUE);
            settings->Release();
        }

        auto* msgHandler = new WebMessageHandler();
        EventRegistrationToken token;
        g_webview->add_WebMessageReceived(msgHandler, &token);
        msgHandler->Release();

        RECT rc;
        GetClientRect(g_hWnd, &rc);
        if (rc.right == 0 || rc.bottom == 0) {
            rc.right = GetSystemMetrics(SM_CXSCREEN);
            rc.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
        g_controller->put_Bounds(rc);
        g_controller->put_IsVisible(TRUE);

        std::wstring url = GetLoadUrl();
        g_webview->Navigate(url.c_str());
        Log("Navigated to %S", url.c_str());

        // 让 WebView2 获得焦点
        g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override { if (--refCount == 0) { delete this; return 0; } return refCount; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
        AddRef();
        return S_OK;
    }
};

struct EnvCompletedHandler : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    HWND hWnd;
    ULONG refCount;
    EnvCompletedHandler(HWND h) : hWnd(h), refCount(1) {}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        if (FAILED(result) || !env) {
            MessageBoxW(hWnd, L"WebView2 运行时未安装。", L"错误", MB_OK);
            PostQuitMessage(1);
            return result;
        }
        auto* ctrlHandler = new CtrlCompletedHandler(hWnd);
        env->CreateCoreWebView2Controller(hWnd, ctrlHandler);
        ctrlHandler->Release();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override { if (--refCount == 0) { delete this; return 0; } return refCount; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
        AddRef();
        return S_OK;
    }
};

void InitWebView(HWND hWnd) {
    wchar_t appData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData);
    std::wstring userDataDir = std::wstring(appData) + L"\\WebWallpaper";
    auto* handler = new EnvCompletedHandler(hWnd);
    CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataDir.c_str(), nullptr, handler);
    handler->Release();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        AddTrayIcon(hWnd);
        InitWebView(hWnd);
        return 0;
    case WM_SIZE:
        if (g_controller) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            g_controller->put_Bounds(rc);
        }
        return 0;
    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) ShowTrayMenu(hWnd);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_RELOAD && g_webview) {
            g_webview->Navigate(GetLoadUrl().c_str());
        } else if (LOWORD(wParam) == IDM_EXIT) {
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_DESTROY:
        RemoveTrayIcon();
        if (g_controller) { g_controller->Close(); g_controller->Release(); }
        if (g_webview) g_webview->Release();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// 占位函数
bool IsWallpaperEngineRunning() { return true; }
HWND GetWorkerW() { return NULL; }

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Web-Wallpaper Debug Console");
    printf("=== Web-Wallpaper Debug Console ===\n");

    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_exeDir = GetExeDir();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 半透明窗口（Alpha = 220，约 86% 不透明）
    g_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,  // 无 WS_EX_NOACTIVATE，无 WS_EX_TRANSPARENT
        CLASS_NAME, L"Web-Wallpaper",
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        NULL, NULL, hInst, NULL);

    if (g_hWnd) {
        SetLayeredWindowAttributes(g_hWnd, 0, 220, LWA_ALPHA);
        SetWindowPos(g_hWnd, HWND_BOTTOM, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetFocus(g_hWnd);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    CloseHandle(hMutex);
    return (int)msg.wParam;
}