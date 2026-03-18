// Web-Wallpaper 桌面主程序
//
// 编译命令：
//   g++ -o wallpaper.exe wallpaper.cpp
//       -I./include -L./x64
//       "x64/WebView2Loader.dll.lib"
//       -lshell32 -lole32 -luser32 -lgdi32 -lws2_32
//       -mwindows -std=c++17
//
// 挂载策略：
//   - 未检测到 Wallpaper Engine → 挂到 WorkerW，桌面图标正常显示在上方
//   - 检测到 Wallpaper Engine   → 透明浮层模式，WE 壁纸透过来，UI 组件浮在上面

#include <winsock2.h>   // 必须在 windows.h 之前
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <WebView2.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ── 常量 ─────────────────────────────────────────────────────────────
#define WM_TRAY         (WM_USER + 1)
#define IDM_RELOAD      1001
#define IDM_EXIT        1002
#define MUTEX_NAME      L"WebWallpaper_SingleInstance"
#define CLASS_NAME      L"WebWallpaperWindow"

// ── 全局变量 ─────────────────────────────────────────────────────────
HWND                              g_hWnd       = NULL;
NOTIFYICONDATAW                   g_nid        = {};
ICoreWebView2Controller*          g_controller = nullptr;
ICoreWebView2*                    g_webview    = nullptr;
std::wstring                      g_exeDir;
bool                              g_devMode        = false;
bool                              g_transparentMode = false;  // 透明模式（跟随 WE）

// ── exe 所在目录 ──────────────────────────────────────────────────────
std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.rfind(L'\\');
    return (pos != std::wstring::npos) ? p.substr(0, pos) : p;
}

// ── 检测开发服务器 ────────────────────────────────────────────────────
bool IsDevServerRunning() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return false; }

    DWORD timeout = 300;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(5173);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bool ok = (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0);
    closesocket(s);
    WSACleanup();
    return ok;
}

std::wstring GetLoadUrl() {
    if (IsDevServerRunning()) {
        g_devMode = true;
        return L"http://localhost:5173";
    }
    g_devMode = false;
    return L"file:///" + g_exeDir + L"\\web\\dist\\index.html";
}

// ── 托盘图标 ─────────────────────────────────────────────────────────
void AddTrayIcon(HWND hWnd) {
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    wcscpy_s(g_nid.szTip, L"Web-Wallpaper");

    std::wstring iconPath = g_exeDir + L"\\icon.ico";
    g_nid.hIcon = (HICON)LoadImageW(
        NULL, iconPath.c_str(), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_LOADFROMFILE
    );
    if (!g_nid.hIcon)
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hWnd) {
    HMENU hMenu = CreatePopupMenu();

    std::wstring modeStr = g_devMode
        ? L"模式：开发 (localhost:5173)"
        : L"模式：生产 (dist)";
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, modeStr.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_RELOAD, L"重新加载");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,   L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                   pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

// ── WebView2 回调（COM 手动实现，不依赖 WRL）─────────────────────────

// WebMessage 回调：接收 Vue 发来的壁纸类型消息
struct WebMessageHandler : ICoreWebView2WebMessageReceivedEventHandler {
    ULONG refCount;
    WebMessageHandler() : refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(
        ICoreWebView2* sender,
        ICoreWebView2WebMessageReceivedEventArgs* args) override
    {
        LPWSTR msg = nullptr;
        args->TryGetWebMessageAsString(&msg);
        if (!msg) return S_OK;

        std::wstring message(msg);
        CoTaskMemFree(msg);

        if (message == L"wallpaper:transparent") {
            g_transparentMode = true;

            // 设置 WebView2 背景完全透明
            // 用显式 IID 避免 MinGW 不支持 __uuidof / IID_PPV_ARGS
            ICoreWebView2Controller2* ctrl2 = nullptr;
            static const GUID IID_ctrl2 = {
                0xc979903e, 0xd4ca, 0x4228,
                {0x92, 0xeb, 0x47, 0xbc, 0x04, 0x17, 0x22, 0x6b}
            };
            if (g_controller &&
                SUCCEEDED(g_controller->QueryInterface(IID_ctrl2, (void**)&ctrl2))) {
                COREWEBVIEW2_COLOR transparent = { 0, 0, 0, 0 };
                ctrl2->put_DefaultBackgroundColor(transparent);
                ctrl2->Release();
            }

            if (g_hWnd)
                SetLayeredWindowAttributes(g_hWnd, RGB(0,0,0), 0, LWA_COLORKEY);

        } else {
            g_transparentMode = false;
            if (g_hWnd)
                SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
        }

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef()  override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--refCount == 0) { delete this; return 0; }
        return refCount;
    }
    // MinGW 不支持 __uuidof，QueryInterface 只处理 IUnknown，
    // WebView2 内部调用时传入的接口指针直接可用，不需要完整实现
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        // 接受任何查询，直接返回 this——MinGW 下 WebView2 回调只需要这样
        *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
        AddRef();
        return S_OK;
    }
};

// Controller 创建完成后的回调（先定义，因为 EnvCompletedHandler 里会用到）
struct CtrlCompletedHandler : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    HWND hWnd;
    ULONG refCount;
    CtrlCompletedHandler(HWND h) : hWnd(h), refCount(1) {}

    HRESULT STDMETHODCALLTYPE Invoke(
        HRESULT result, ICoreWebView2Controller* controller) override
    {
        if (FAILED(result) || !controller) return result;

        g_controller = controller;
        g_controller->AddRef();
        g_controller->get_CoreWebView2(&g_webview);

        // 关闭右键菜单、状态栏（开发模式保留 DevTools）
        ICoreWebView2Settings* settings = nullptr;
        g_webview->get_Settings(&settings);
        if (settings) {
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_IsWebMessageEnabled(TRUE);  // 启用 postMessage 通信
            if (!g_devMode)
                settings->put_AreDevToolsEnabled(FALSE);
            settings->Release();
        }

        // 注册 WebMessage 接收回调
        auto* msgHandler = new WebMessageHandler();
        EventRegistrationToken token;
        g_webview->add_WebMessageReceived(msgHandler, &token);
        msgHandler->Release();

        // 铺满窗口
        RECT rc;
        GetClientRect(hWnd, &rc);
        g_controller->put_Bounds(rc);
        g_controller->put_IsVisible(TRUE);

        // 加载页面
        std::wstring url = GetLoadUrl();
        g_webview->Navigate(url.c_str());

        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef()  override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--refCount == 0) { delete this; return 0; }
        return refCount;
    }
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

    HRESULT STDMETHODCALLTYPE Invoke(
        HRESULT result, ICoreWebView2Environment* env) override
    {
        if (FAILED(result) || !env) {
            MessageBoxW(hWnd,
                L"WebView2 运行时未安装。\n"
                L"请访问以下地址下载安装：\n"
                L"https://developer.microsoft.com/microsoft-edge/webview2/",
                L"错误", MB_OK | MB_ICONERROR);
            PostQuitMessage(1);
            return result;
        }

        auto* ctrlHandler = new CtrlCompletedHandler(hWnd);
        env->CreateCoreWebView2Controller(hWnd, ctrlHandler);
        ctrlHandler->Release();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef()  override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        if (--refCount == 0) { delete this; return 0; }
        return refCount;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
        AddRef();
        return S_OK;
    }
};

// ── WebView2 初始化 ───────────────────────────────────────────────────
void InitWebView(HWND hWnd) {
    wchar_t appData[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData);
    std::wstring userDataDir = std::wstring(appData) + L"\\WebWallpaper";

    auto* handler = new EnvCompletedHandler(hWnd);
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataDir.c_str(), nullptr, handler);
    handler->Release();
}

// ── 窗口过程 ─────────────────────────────────────────────────────────
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
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_RELOAD:
            if (g_webview) {
                std::wstring url = GetLoadUrl();
                g_webview->Navigate(url.c_str());
            }
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        RemoveTrayIcon();
        if (g_controller) { g_controller->Close(); g_controller->Release(); }
        if (g_webview)     { g_webview->Release(); }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── WorkerW 挂载 ──────────────────────────────────────────────────────

// 检测 Wallpaper Engine 是否在运行（检查进程名）
bool IsWallpaperEngineRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    bool found = false;

    if (Process32FirstW(snap, &pe)) {
        do {
            // Wallpaper Engine 主进程名
            if (wcscmp(pe.szExeFile, L"wallpaper_engine.exe") == 0 ||
                wcscmp(pe.szExeFile, L"wallpaper32.exe") == 0 ||
                wcscmp(pe.szExeFile, L"wallpaper64.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

// 枚举回调：找到带 SHELLDLL_DefView 的窗口，取其后的 WorkerW
struct FindWorkerWData { HWND result; };

BOOL CALLBACK FindWorkerWProc(HWND hWnd, LPARAM lParam) {
    FindWorkerWData* data = (FindWorkerWData*)lParam;
    HWND hShelf = FindWindowExW(hWnd, NULL, L"SHELLDLL_DefView", NULL);
    if (hShelf) {
        data->result = FindWindowExW(NULL, hWnd, L"WorkerW", NULL);
        return FALSE;  // 找到了，停止枚举
    }
    return TRUE;
}

// 找到 WorkerW 句柄，找不到返回 NULL
HWND GetWorkerW() {
    // 1. 找 Progman
    HWND hProgman = FindWindowW(L"Progman", NULL);
    if (!hProgman) return NULL;

    // 2. 发送 0x052C 让系统在 Progman 和图标层之间创建 WorkerW
    SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

    // 3. 枚举顶层窗口找到 WorkerW
    FindWorkerWData data = { NULL };
    EnumWindows(FindWorkerWProc, (LPARAM)&data);
    return data.result;
}

// ── 入口点 ────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // 单例检查
    HANDLE hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    g_exeDir = GetExeDir();

    // 注册窗口类
    WNDCLASSEXW wc    = {};
    wc.cbSize         = sizeof(wc);
    wc.lpfnWndProc    = WndProc;
    wc.hInstance      = hInst;
    wc.lpszClassName  = CLASS_NAME;
    wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    // 虚拟桌面尺寸
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 检测 Wallpaper Engine
    bool weRunning = IsWallpaperEngineRunning();

    if (!weRunning) {
        // ── WorkerW 模式：挂到桌面层，图标在上面正常显示 ──
        HWND hWorkerW = GetWorkerW();

        if (hWorkerW) {
            // 创建子窗口，挂到 WorkerW 下
            g_hWnd = CreateWindowExW(
                0,
                CLASS_NAME, L"Web-Wallpaper",
                WS_CHILD | WS_VISIBLE,
                0, 0, w, h,
                hWorkerW, NULL, hInst, NULL
            );
        } else {
            // WorkerW 找不到（极少数情况），退回底层窗口模式
            g_hWnd = CreateWindowExW(
                WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                CLASS_NAME, L"Web-Wallpaper",
                WS_POPUP | WS_VISIBLE,
                x, y, w, h,
                NULL, NULL, hInst, NULL
            );
            SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
            SetWindowPos(g_hWnd, HWND_BOTTOM, x, y, w, h,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    } else {
        // ── 透明浮层模式：WE 在运行，浮在上面只显示 UI 组件 ──
        // 通知 Vue 切换透明模式（Vue 加载后会读 localStorage，
        // 这里提前设标志，CtrlCompletedHandler 里会检查）
        g_transparentMode = true;

        g_hWnd = CreateWindowExW(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CLASS_NAME, L"Web-Wallpaper",
            WS_POPUP | WS_VISIBLE,
            x, y, w, h,
            NULL, NULL, hInst, NULL
        );
        // 透明背景，让 WE 壁纸透过来
        SetLayeredWindowAttributes(g_hWnd, RGB(0,0,0), 0, LWA_COLORKEY);
        SetWindowPos(g_hWnd, HWND_BOTTOM, x, y, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
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