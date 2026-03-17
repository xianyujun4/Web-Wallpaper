// Web-Wallpaper Installer
// 编译命令：g++ -o installer.exe installer.cpp -lwininet -lshell32 -lshlwapi -lole32 -mwindows
//
// 功能：
//   1. 让用户选择安装目录（默认 C:\WebWallpaper）
//   2. 检查 Node.js 和 g++ 环境
//   3. 从 GitHub 下载仓库 zip（main 分支）
//   4. 解压到安装目录
//   5. 编译 runner.cpp → runner.exe
//   6. npm install + npm run build（在 web/ 子目录）
//   7. 写注册表（mycmd:// 协议）
//   8. 完成提示

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// ── 控件 ID ──────────────────────────────────────────────────────────
#define IDC_DIR_EDIT      101
#define IDC_DIR_BROWSE    102
#define IDC_INSTALL_BTN   103
#define IDC_CANCEL_BTN    104
#define IDC_LOG           105
#define IDC_PROGRESS_TEXT 106

// ── 全局变量 ─────────────────────────────────────────────────────────
HWND g_hWnd       = NULL;
HWND g_hLog       = NULL;
HWND g_hDirEdit   = NULL;
HWND g_hInstallBtn = NULL;
bool g_installing = false;

// ── 工具函数 ─────────────────────────────────────────────────────────

// 向日志框追加一行
void AppendLog(const std::wstring& msg) {
    if (!g_hLog) return;
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)(msg + L"\r\n").c_str());
    SendMessageW(g_hLog, EM_SCROLL, SB_BOTTOM, 0);
}

// wstring ↔ string（UTF-8）
std::wstring s2w(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], n);
    return r;
}
std::string w2s(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &r[0], n, NULL, NULL);
    return r;
}

// 执行命令并等待，返回退出码；输出重定向到日志框
int RunCommand(const std::wstring& cmd, const std::wstring& workDir = L"") {
    AppendLog(L"  > " + cmd);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;

    PROCESS_INFORMATION pi = {};

    std::wstring cmdBuf = cmd;
    bool ok = CreateProcessW(
        NULL, &cmdBuf[0], NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL,
        workDir.empty() ? NULL : workDir.c_str(),
        &si, &pi
    );

    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        AppendLog(L"  [错误] 无法启动进程");
        return -1;
    }

    // 读取输出
    char buf[512];
    DWORD read;
    std::string lineBuf;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &read, NULL) && read > 0) {
        buf[read] = '\0';
        lineBuf += buf;
        // 按行拆分输出到日志
        size_t pos;
        while ((pos = lineBuf.find('\n')) != std::string::npos) {
            std::string line = lineBuf.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) AppendLog(L"    " + s2w(line));
            lineBuf = lineBuf.substr(pos + 1);
        }
        // 让 UI 保持响应
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}

// 检查命令是否存在（用 where 命令）
bool CheckTool(const std::wstring& tool) {
    std::wstring cmd = L"cmd.exe /c where " + tool + L" >nul 2>&1";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::wstring buf = cmd;
    if (!CreateProcessW(NULL, &buf[0], NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return false;

    WaitForSingleObject(pi.hProcess, 5000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

// 从 GitHub 下载文件到本地路径
bool DownloadFile(const std::wstring& url, const std::wstring& destPath) {
    AppendLog(L"  下载: " + url);

    HINTERNET hInternet = InternetOpenW(
        L"WebWallpaper-Installer/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0
    );
    if (!hInternet) { AppendLog(L"  [错误] 无法初始化网络"); return false; }

    HINTERNET hUrl = InternetOpenUrlW(
        hInternet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE,
        0
    );
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        AppendLog(L"  [错误] 无法打开 URL，请检查网络连接");
        return false;
    }

    std::ofstream file(destPath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        AppendLog(L"  [错误] 无法写入文件: " + destPath);
        return false;
    }

    char buf[8192];
    DWORD read;
    DWORD total = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
        file.write(buf, read);
        total += read;
        // 每 100KB 刷新一次日志
        if (total % (100 * 1024) < sizeof(buf)) {
            AppendLog(L"  已下载: " + std::to_wstring(total / 1024) + L" KB");
        }
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    file.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    AppendLog(L"  下载完成，共 " + std::to_wstring(total / 1024) + L" KB");
    return total > 0;
}

// 解压 zip 到目标目录（用 PowerShell，Windows 自带）
bool UnzipFile(const std::wstring& zipPath, const std::wstring& destDir) {
    std::wstring cmd = L"powershell.exe -NoProfile -Command "
        L"\"Expand-Archive -Force -LiteralPath '" + zipPath +
        L"' -DestinationPath '" + destDir + L"'\"";
    int code = RunCommand(cmd);
    return code == 0;
}

// 写注册表：注册 mycmd:// 协议
bool WriteRegistry(const std::wstring& runnerPath) {
    // HKEY_CURRENT_USER\Software\Classes\mycmd
    const std::wstring keyBase = L"Software\\Classes\\mycmd";

    auto setStr = [](const std::wstring& key, const std::wstring& name,
                     const std::wstring& val) -> bool {
        HKEY hKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, NULL,
                            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                            &hKey, NULL) != ERROR_SUCCESS)
            return false;
        bool ok = RegSetValueExW(hKey, name.empty() ? NULL : name.c_str(),
                                 0, REG_SZ,
                                 (const BYTE*)val.c_str(),
                                 (DWORD)((val.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return ok;
    };

    // mycmd\ 默认值
    if (!setStr(keyBase, L"", L"URL:mycmd Protocol")) return false;
    // mycmd\ URL Protocol
    if (!setStr(keyBase, L"URL Protocol", L"")) return false;
    // mycmd\shell\open\command 默认值
    std::wstring cmdVal = L"\"" + runnerPath + L"\" \"%1\"";
    if (!setStr(keyBase + L"\\shell\\open\\command", L"", cmdVal)) return false;

    return true;
}

// ── 安装主流程（在独立线程里跑，避免阻塞 UI）────────────────────────

struct InstallParams {
    std::wstring installDir;
};

DWORD WINAPI InstallThread(LPVOID param) {
    InstallParams* p = (InstallParams*)param;
    std::wstring installDir = p->installDir;
    delete p;

    EnableWindow(g_hInstallBtn, FALSE);

    // 确保安装目录存在
    AppendLog(L"[1/7] 创建安装目录...");
    if (!CreateDirectoryW(installDir.c_str(), NULL) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        AppendLog(L"  [错误] 无法创建目录: " + installDir);
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  安装目录: " + installDir);

    // 检查 Node.js
    AppendLog(L"[2/7] 检查环境...");
    if (!CheckTool(L"node")) {
        AppendLog(L"  [错误] 未找到 Node.js，请先安装：https://nodejs.org");
        ShellExecuteW(NULL, L"open", L"https://nodejs.org", NULL, NULL, SW_SHOW);
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  Node.js ✓");

    if (!CheckTool(L"g++")) {
        AppendLog(L"  [错误] 未找到 g++，请先安装 MSYS2 并配置环境变量：https://www.msys2.org");
        ShellExecuteW(NULL, L"open", L"https://www.msys2.org", NULL, NULL, SW_SHOW);
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  g++ ✓");

    // 下载 zip
    AppendLog(L"[3/7] 从 GitHub 下载源码...");
    std::wstring zipUrl   = L"https://github.com/xianyujun4/Web-Wallpaper/archive/refs/heads/main.zip";
    std::wstring zipPath  = installDir + L"\\source.zip";
    if (!DownloadFile(zipUrl, zipPath)) {
        AppendLog(L"  [错误] 下载失败，请检查网络或稍后重试");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }

    // 解压
    AppendLog(L"[4/7] 解压文件...");
    std::wstring extractDir = installDir + L"\\__extract";
    if (!UnzipFile(zipPath, extractDir)) {
        AppendLog(L"  [错误] 解压失败");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }

    // GitHub zip 解压后有一层 "Web-Wallpaper-main" 子目录，移动到安装目录
    std::wstring srcRoot = extractDir + L"\\Web-Wallpaper-main";
    // 用 robocopy 移动文件（robocopy Windows 自带）
    std::wstring moveCmd = L"cmd.exe /c robocopy \"" + srcRoot + L"\" \"" +
                           installDir + L"\" /E /MOVE /NFL /NDL /NJH /NJS >nul";
    RunCommand(moveCmd);

    // 删除临时文件
    DeleteFileW(zipPath.c_str());
    RemoveDirectoryW(extractDir.c_str());

    // 编译 runner.cpp
    AppendLog(L"[5/7] 编译 runner.exe...");
    std::wstring runnerSrc = installDir + L"\\runner.cpp";
    std::wstring runnerExe = installDir + L"\\runner.exe";
    std::wstring compileCmd = L"g++ -o \"" + runnerExe + L"\" \"" + runnerSrc +
                              L"\" -lshell32 -mwindows";
    int compileCode = RunCommand(compileCmd, installDir);
    if (compileCode != 0) {
        AppendLog(L"  [错误] 编译失败，请确认 g++ 版本支持 C++17");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  runner.exe 编译完成 ✓");

    // npm install + build
    AppendLog(L"[6/7] 安装依赖并构建前端...");
    std::wstring webDir = installDir + L"\\web";

    int npmInstall = RunCommand(L"cmd.exe /c npm install", webDir);
    if (npmInstall != 0) {
        AppendLog(L"  [错误] npm install 失败");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  npm install ✓");

    int npmBuild = RunCommand(L"cmd.exe /c npm run build", webDir);
    if (npmBuild != 0) {
        AppendLog(L"  [错误] npm run build 失败");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  npm run build ✓");

    // 写注册表
    AppendLog(L"[7/7] 注册 mycmd:// 协议...");
    if (!WriteRegistry(runnerExe)) {
        AppendLog(L"  [错误] 注册表写入失败");
        EnableWindow(g_hInstallBtn, TRUE);
        return 1;
    }
    AppendLog(L"  注册表写入完成 ✓");

    // 完成
    AppendLog(L"");
    AppendLog(L"══════════════════════════════════════");
    AppendLog(L"  安装完成！");
    AppendLog(L"");
    AppendLog(L"  前端文件位于：" + webDir + L"\\dist");
    AppendLog(L"  runner.exe 位于：" + runnerExe);
    AppendLog(L"");
    AppendLog(L"  ⚠ 请勿移动安装目录，否则 mycmd:// 协议将失效。");
    AppendLog(L"  如需移动，请重新运行 installer.exe。");
    AppendLog(L"══════════════════════════════════════");

    MessageBoxW(g_hWnd,
        (L"安装完成！\n\n"
         L"前端文件：" + webDir + L"\\dist\n"
         L"runner.exe：" + runnerExe + L"\n\n"
         L"⚠ 请勿移动安装目录，否则协议注册将失效。").c_str(),
        L"安装完成",
        MB_OK | MB_ICONINFORMATION
    );

    EnableWindow(g_hInstallBtn, TRUE);
    return 0;
}

// ── Win32 窗口过程 ────────────────────────────────────────────────────

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hWnd;

        // 标题
        CreateWindowW(L"STATIC", L"Web-Wallpaper 安装程序",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            20, 15, 460, 25, hWnd, NULL, NULL, NULL);

        // 安装目录标签
        CreateWindowW(L"STATIC", L"安装目录：",
            WS_VISIBLE | WS_CHILD,
            20, 55, 80, 20, hWnd, NULL, NULL, NULL);

        // 目录输入框
        g_hDirEdit = CreateWindowW(L"EDIT", L"C:\\WebWallpaper",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            105, 52, 290, 22, hWnd, (HMENU)IDC_DIR_EDIT, NULL, NULL);

        // 浏览按钮
        CreateWindowW(L"BUTTON", L"浏览...",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            405, 51, 65, 24, hWnd, (HMENU)IDC_DIR_BROWSE, NULL, NULL);

        // 日志框
        g_hLog = CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            20, 90, 460, 280, hWnd, (HMENU)IDC_LOG, NULL, NULL);

        // 安装按钮
        g_hInstallBtn = CreateWindowW(L"BUTTON", L"开始安装",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            300, 385, 85, 30, hWnd, (HMENU)IDC_INSTALL_BTN, NULL, NULL);

        // 取消按钮
        CreateWindowW(L"BUTTON", L"取消",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            395, 385, 85, 30, hWnd, (HMENU)IDC_CANCEL_BTN, NULL, NULL);

        AppendLog(L"Web-Wallpaper 安装程序");
        AppendLog(L"─────────────────────────────────────");
        AppendLog(L"安装前请确认已安装：");
        AppendLog(L"  • Node.js  （https://nodejs.org）");
        AppendLog(L"  • g++ / MinGW  （https://www.msys2.org）");
        AppendLog(L"");
        AppendLog(L"选择安装目录后点击「开始安装」。");
        AppendLog(L"安装完成后请勿移动目录，否则需重新安装。");
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == IDC_DIR_BROWSE) {
            // 弹出目录选择对话框
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"选择安装目录";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, path)) {
                    SetWindowTextW(g_hDirEdit, path);
                }
                CoTaskMemFree(pidl);
            }
        }

        if (id == IDC_INSTALL_BTN && !g_installing) {
            wchar_t dir[MAX_PATH];
            GetWindowTextW(g_hDirEdit, dir, MAX_PATH);

            if (wcslen(dir) == 0) {
                MessageBoxW(hWnd, L"请选择安装目录", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }

            // 确认
            std::wstring msg = std::wstring(L"将安装到：\n") + dir +
                               L"\n\n确认开始安装？";
            if (MessageBoxW(hWnd, msg.c_str(), L"确认安装",
                            MB_YESNO | MB_ICONQUESTION) != IDYES)
                break;

            g_installing = true;
            // 清空日志
            SetWindowTextW(g_hLog, L"");
            AppendLog(L"开始安装...");

            InstallParams* p = new InstallParams();
            p->installDir = dir;
            HANDLE hThread = CreateThread(NULL, 0, InstallThread, p, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }

        if (id == IDC_CANCEL_BTN) {
            if (g_installing) {
                if (MessageBoxW(hWnd, L"安装正在进行中，确定要退出吗？",
                                L"确认", MB_YESNO | MB_ICONWARNING) != IDYES)
                    break;
            }
            PostQuitMessage(0);
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── 入口点 ────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // 初始化 COM（SHBrowseForFolder 需要）
    CoInitialize(NULL);

    const wchar_t* CLASS_NAME = L"WebWallpaperInstaller";

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(
        CLASS_NAME, L"Web-Wallpaper 安装程序",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 460,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}