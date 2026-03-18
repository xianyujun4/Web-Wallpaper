// Web-Wallpaper Installer
// 编译命令：g++ -o installer.exe installer.cpp -lwininet -lshell32 -lshlwapi -lole32 -mwindows
//
// 安装步骤：
//   1.  选择安装目录
//   2.  检查 Node.js / g++ 环境
//   3.  从 GitHub 下载源码 zip
//   4.  解压源码
//   5.  下载 WebView2 SDK（NuGet 包）
//   6.  解压 WebView2 SDK
//   7.  编译 runner.exe
//   8.  编译 wallpaper.exe
//   9.  npm install + npm run build
//   10. 写注册表（mycmd:// 协议 + 可选开机启动）

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <fstream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// ── 控件 ID ──────────────────────────────────────────────────────────
#define IDC_DIR_EDIT        101
#define IDC_DIR_BROWSE      102
#define IDC_INSTALL_BTN     103
#define IDC_CANCEL_BTN      104
#define IDC_LOG             105
#define IDC_AUTOSTART_CHK   106
#define IDC_SRC_OFFICIAL    107
#define IDC_SRC_MIRROR      108

// ── 全局变量 ─────────────────────────────────────────────────────────
HWND g_hWnd          = NULL;
HWND g_hLog          = NULL;
HWND g_hDirEdit      = NULL;
HWND g_hInstallBtn   = NULL;
HWND g_hAutoStartChk = NULL;
HWND g_hSrcOfficial  = NULL;
HWND g_hSrcMirror    = NULL;
bool g_installing    = false;

// ── 工具函数 ─────────────────────────────────────────────────────────

void AppendLog(const std::wstring& msg) {
    if (!g_hLog) return;
    int len = GetWindowTextLengthW(g_hLog);
    SendMessageW(g_hLog, EM_SETSEL, len, len);
    SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)(msg + L"\r\n").c_str());
    SendMessageW(g_hLog, EM_SCROLL, SB_BOTTOM, 0);
}

std::wstring s2w(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &r[0], n);
    return r;
}

// 执行命令，实时输出到日志，返回退出码
int RunCommand(const std::wstring& cmd, const std::wstring& workDir = L"") {
    AppendLog(L"  > " + cmd);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;

    PROCESS_INFORMATION pi = {};
    std::wstring buf = cmd;
    bool ok = CreateProcessW(NULL, &buf[0], NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL,
                             workDir.empty() ? NULL : workDir.c_str(),
                             &si, &pi);
    CloseHandle(hWrite);

    if (!ok) { CloseHandle(hRead); AppendLog(L"  [错误] 无法启动进程"); return -1; }

    char rbuf[512];
    DWORD read;
    std::string line;
    while (ReadFile(hRead, rbuf, sizeof(rbuf) - 1, &read, NULL) && read > 0) {
        rbuf[read] = '\0';
        line += rbuf;
        size_t pos;
        while ((pos = line.find('\n')) != std::string::npos) {
            std::string l = line.substr(0, pos);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (!l.empty()) AppendLog(L"    " + s2w(l));
            line = line.substr(pos + 1);
        }
        MSG m;
        while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
    }
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

// 检查工具是否在 PATH 里
bool CheckTool(const std::wstring& tool) {
    std::wstring cmd = L"cmd.exe /c where " + tool + L" >nul 2>&1";
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::wstring buf = cmd;
    if (!CreateProcessW(NULL, &buf[0], NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 5000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
}

// 下载文件到本地路径
bool DownloadFile(const std::wstring& url, const std::wstring& destPath) {
    AppendLog(L"  下载: " + url);

    HINTERNET hNet = InternetOpenW(L"WebWallpaper-Installer/1.0",
                                   INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hNet) { AppendLog(L"  [错误] 无法初始化网络"); return false; }

    HINTERNET hUrl = InternetOpenUrlW(hNet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hNet);
        AppendLog(L"  [错误] 无法打开 URL");
        return false;
    }

    std::ofstream file(destPath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        InternetCloseHandle(hUrl); InternetCloseHandle(hNet);
        AppendLog(L"  [错误] 无法写入文件");
        return false;
    }

    char buf[8192];
    DWORD read, total = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &read) && read > 0) {
        file.write(buf, read);
        total += read;
        if (total % (200 * 1024) < (DWORD)sizeof(buf))
            AppendLog(L"  已下载: " + std::to_wstring(total / 1024) + L" KB");
        MSG m;
        while (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessage(&m); }
    }
    file.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hNet);
    AppendLog(L"  完成，共 " + std::to_wstring(total / 1024) + L" KB");
    return total > 0;
}

// 用 PowerShell Expand-Archive 解压
bool UnzipFile(const std::wstring& zipPath, const std::wstring& destDir) {
    std::wstring cmd = L"powershell.exe -NoProfile -Command "
        L"\"Expand-Archive -Force -LiteralPath '" + zipPath +
        L"' -DestinationPath '" + destDir + L"'\"";
    return RunCommand(cmd) == 0;
}

// 写 mycmd:// 协议注册表
bool WriteRegistryMycmd(const std::wstring& runnerPath) {
    auto set = [](const std::wstring& key, const std::wstring& name,
                  const std::wstring& val) -> bool {
        HKEY h;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, NULL,
                            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &h, NULL) != ERROR_SUCCESS)
            return false;
        bool ok = RegSetValueExW(h, name.empty() ? NULL : name.c_str(), 0, REG_SZ,
                                 (const BYTE*)val.c_str(),
                                 (DWORD)((val.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        RegCloseKey(h);
        return ok;
    };
    const std::wstring base = L"Software\\Classes\\mycmd";
    if (!set(base, L"", L"URL:mycmd Protocol")) return false;
    if (!set(base, L"URL Protocol", L"")) return false;
    if (!set(base + L"\\shell\\open\\command", L"",
             L"\"" + runnerPath + L"\" \"%1\"")) return false;
    return true;
}

// 写开机启动项（HKCU Run）
bool WriteRegistryAutoStart(const std::wstring& wallpaperExe) {
    HKEY h;
    const std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, KEY_WRITE, &h) != ERROR_SUCCESS)
        return false;
    std::wstring val = L"\"" + wallpaperExe + L"\"";
    bool ok = RegSetValueExW(h, L"WebWallpaper", 0, REG_SZ,
                             (const BYTE*)val.c_str(),
                             (DWORD)((val.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(h);
    return ok;
}

// ── 安装线程 ──────────────────────────────────────────────────────────
struct InstallParams {
    std::wstring installDir;
    bool         autoStart;
    bool         useMirror;
};

DWORD WINAPI InstallThread(LPVOID param) {
    InstallParams* p = (InstallParams*)param;
    std::wstring dir       = p->installDir;
    bool         autoStart = p->autoStart;
    bool         useMirror = p->useMirror;
    delete p;

    // 根据用户选择决定下载前缀
    const std::wstring MIRROR_PREFIX = L"https://ghproxy.com/";
    auto makeUrl = [&](const std::wstring& githubUrl) -> std::wstring {
        return useMirror ? (MIRROR_PREFIX + githubUrl) : githubUrl;
    };

    EnableWindow(g_hInstallBtn, FALSE);

    // 1. 创建安装目录
    AppendLog(L"[1/10] 创建安装目录...");
    if (!CreateDirectoryW(dir.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        AppendLog(L"  [错误] 无法创建目录: " + dir);
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  " + dir);

    // 2. 检查环境
    AppendLog(L"[2/10] 检查环境...");
    if (!CheckTool(L"node")) {
        AppendLog(L"  [错误] 未找到 Node.js，请先安装后重试");
        ShellExecuteW(NULL, L"open", L"https://nodejs.org", NULL, NULL, SW_SHOW);
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  Node.js ✓");

    if (!CheckTool(L"g++")) {
        AppendLog(L"  [错误] 未找到 g++，请安装 MSYS2 并将 MinGW bin 加入 PATH");
        ShellExecuteW(NULL, L"open", L"https://www.msys2.org", NULL, NULL, SW_SHOW);
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  g++ ✓");

    // 检测 WebView2 运行时（查注册表）
    {
        HKEY h = NULL;
        const wchar_t* wv2Key =
            L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients"
            L"\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
        bool found =
            RegOpenKeyExW(HKEY_LOCAL_MACHINE, wv2Key, 0, KEY_READ, &h) == ERROR_SUCCESS;
        if (h) { RegCloseKey(h); h = NULL; }
        if (!found)
            found = RegOpenKeyExW(HKEY_CURRENT_USER, wv2Key, 0, KEY_READ, &h) == ERROR_SUCCESS;
        if (h) RegCloseKey(h);

        if (!found) {
            AppendLog(L"  [错误] 未检测到 WebView2 运行时");
            AppendLog(L"  正在打开下载页，安装完成后请重新运行 installer...");
            ShellExecuteW(NULL, L"open",
                L"https://developer.microsoft.com/microsoft-edge/webview2/",
                NULL, NULL, SW_SHOW);
            EnableWindow(g_hInstallBtn, TRUE); return 1;
        }
        AppendLog(L"  WebView2 运行时 ✓");
    }

    // 3. 下载源码
    AppendLog(L"[3/10] 下载源码...");
    AppendLog(useMirror ? L"  使用镜像源 (ghproxy.com)" : L"  使用官方源 (github.com)");
    std::wstring srcZip = dir + L"\\source.zip";
    if (!DownloadFile(
        makeUrl(L"https://github.com/xianyujun4/Web-Wallpaper/archive/refs/heads/main.zip"),
        srcZip)) {
        AppendLog(L"  [错误] 下载失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }

    // 4. 解压源码
    AppendLog(L"[4/10] 解压源码...");
    std::wstring srcExtract = dir + L"\\__src";
    if (!UnzipFile(srcZip, srcExtract)) {
        AppendLog(L"  [错误] 解压失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    RunCommand(L"cmd.exe /c robocopy \"" + srcExtract + L"\\Web-Wallpaper-main\" \""
               + dir + L"\" /E /MOVE /NFL /NDL /NJH /NJS >nul");
    DeleteFileW(srcZip.c_str());
    RunCommand(L"cmd.exe /c rmdir /s /q \"" + srcExtract + L"\"");
    AppendLog(L"  源码就绪 ✓");

    // 5. 下载 WebView2 SDK
    AppendLog(L"[5/10] 下载 WebView2 SDK...");
    AppendLog(L"  （NuGet 官方源，不受镜像设置影响）");
    std::wstring sdkZip = dir + L"\\webview2.zip";
    // NuGet 包本质是 zip，直接下载最新版
    if (!DownloadFile(
        L"https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2",
        sdkZip)) {
        AppendLog(L"  [错误] SDK 下载失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }

    // 6. 解压 SDK
    AppendLog(L"[6/10] 解压 WebView2 SDK...");
    std::wstring sdkDir = dir + L"\\sdk\\WebView2";
    CreateDirectoryW((dir + L"\\sdk").c_str(), NULL);
    CreateDirectoryW(sdkDir.c_str(), NULL);
    if (!UnzipFile(sdkZip, sdkDir)) {
        AppendLog(L"  [错误] SDK 解压失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    DeleteFileW(sdkZip.c_str());
    std::wstring sdkInc = sdkDir + L"\\build\\native\\include";
    std::wstring sdkLib = sdkDir + L"\\build\\native\\x64";
    AppendLog(L"  WebView2 SDK 就绪 ✓");

    // 7. 编译 runner.exe
    AppendLog(L"[7/10] 编译 runner.exe...");
    std::wstring runnerExe = dir + L"\\runner.exe";
    if (RunCommand(L"g++ -o \"" + runnerExe + L"\" \""
                   + dir + L"\\runner.cpp\" -lshell32 -mwindows", dir) != 0) {
        AppendLog(L"  [错误] 编译失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  runner.exe ✓");

    // 8. 编译 wallpaper.exe
    AppendLog(L"[8/10] 编译 wallpaper.exe...");
    std::wstring wallpaperExe = dir + L"\\wallpaper.exe";

    // 把 WebView2Loader.dll 复制到安装目录（运行时依赖）
    CopyFileW((sdkLib + L"\\WebView2Loader.dll").c_str(),
              (dir    + L"\\WebView2Loader.dll").c_str(), FALSE);

    std::wstring loaderLib = sdkLib + L"\\WebView2Loader.dll.lib";
    std::wstring wpCmd =
        L"g++ -o \"" + wallpaperExe + L"\""
        L" \"" + dir + L"\\wallpaper.cpp\""
        L" -I\"" + sdkInc + L"\""
        L" -L\"" + sdkLib + L"\""
        L" \"" + loaderLib + L"\""
        L" -lshell32 -lole32 -luser32 -lgdi32 -lws2_32"
        L" -mwindows -std=c++17";
    if (RunCommand(wpCmd, dir) != 0) {
        AppendLog(L"  [错误] 编译失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  wallpaper.exe ✓");

    // 9. npm install + build
    AppendLog(L"[9/10] 构建前端...");
    std::wstring webDir = dir + L"\\web";
    if (RunCommand(L"cmd.exe /c npm install", webDir) != 0) {
        AppendLog(L"  [错误] npm install 失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  npm install ✓");
    if (RunCommand(L"cmd.exe /c npm run build", webDir) != 0) {
        AppendLog(L"  [错误] npm run build 失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  npm run build ✓");

    // 10. 写注册表
    AppendLog(L"[10/10] 写入注册表...");
    if (!WriteRegistryMycmd(runnerExe)) {
        AppendLog(L"  [错误] mycmd:// 协议注册失败");
        EnableWindow(g_hInstallBtn, TRUE); return 1;
    }
    AppendLog(L"  mycmd:// 协议 ✓");

    if (autoStart) {
        if (WriteRegistryAutoStart(wallpaperExe))
            AppendLog(L"  开机启动 ✓");
        else
            AppendLog(L"  [警告] 开机启动项写入失败，可手动添加");
    }

    // 完成
    AppendLog(L"");
    AppendLog(L"══════════════════════════════════════════");
    AppendLog(L"  安装完成！");
    AppendLog(L"  壁纸程序：" + wallpaperExe);
    AppendLog(L"  前端：" + webDir + L"\\dist");
    AppendLog(autoStart ? L"  开机启动：已启用" : L"  开机启动：未启用");
    AppendLog(L"  ⚠ 请勿移动安装目录");
    AppendLog(L"══════════════════════════════════════════");

    MessageBoxW(g_hWnd,
        (std::wstring(L"安装完成！\n\n壁纸程序：") + wallpaperExe +
         L"\n\n" + (autoStart
             ? L"已设置开机自动启动，重启后生效。"
             : L"未设置开机启动，需手动运行 wallpaper.exe。") +
         L"\n\n⚠ 请勿移动安装目录。").c_str(),
        L"安装完成", MB_OK | MB_ICONINFORMATION);

    EnableWindow(g_hInstallBtn, TRUE);
    g_installing = false;
    return 0;
}

// ── 窗口过程 ──────────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hWnd;

        CreateWindowW(L"STATIC", L"Web-Wallpaper 安装程序",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            20, 15, 460, 25, hWnd, NULL, NULL, NULL);

        CreateWindowW(L"STATIC", L"安装目录：",
            WS_VISIBLE | WS_CHILD,
            20, 55, 80, 20, hWnd, NULL, NULL, NULL);

        g_hDirEdit = CreateWindowW(L"EDIT", L"C:\\WebWallpaper",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            105, 52, 275, 22, hWnd, (HMENU)IDC_DIR_EDIT, NULL, NULL);

        CreateWindowW(L"BUTTON", L"浏览...",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            390, 51, 80, 24, hWnd, (HMENU)IDC_DIR_BROWSE, NULL, NULL);

        // 开机启动复选框（默认勾选）
        g_hAutoStartChk = CreateWindowW(L"BUTTON",
            L"随 Windows 启动（推荐）",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            20, 85, 220, 22, hWnd, (HMENU)IDC_AUTOSTART_CHK, NULL, NULL);
        SendMessageW(g_hAutoStartChk, BM_SETCHECK, BST_CHECKED, 0);

        // 下载源选择
        CreateWindowW(L"STATIC", L"下载源：",
            WS_VISIBLE | WS_CHILD,
            20, 112, 55, 20, hWnd, NULL, NULL, NULL);

        g_hSrcOfficial = CreateWindowW(L"BUTTON", L"官方源 (GitHub)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            80, 110, 150, 22, hWnd, (HMENU)IDC_SRC_OFFICIAL, NULL, NULL);

        g_hSrcMirror = CreateWindowW(L"BUTTON", L"镜像源 (ghproxy，国内推荐)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            240, 110, 230, 22, hWnd, (HMENU)IDC_SRC_MIRROR, NULL, NULL);

        // 默认选镜像源
        SendMessageW(g_hSrcMirror, BM_SETCHECK, BST_CHECKED, 0);

        g_hLog = CreateWindowW(L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            20, 140, 460, 240, hWnd, (HMENU)IDC_LOG, NULL, NULL);

        g_hInstallBtn = CreateWindowW(L"BUTTON", L"开始安装",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            295, 395, 90, 28, hWnd, (HMENU)IDC_INSTALL_BTN, NULL, NULL);

        CreateWindowW(L"BUTTON", L"取消",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            395, 395, 80, 28, hWnd, (HMENU)IDC_CANCEL_BTN, NULL, NULL);

        AppendLog(L"Web-Wallpaper 安装程序");
        AppendLog(L"──────────────────────────────────────────");
        AppendLog(L"安装前请确认以下工具已安装并加入 PATH：");
        AppendLog(L"  • Node.js     https://nodejs.org");
        AppendLog(L"  • g++ / MinGW https://www.msys2.org");
        AppendLog(L"");
        AppendLog(L"WebView2 SDK 将在安装过程中自动下载。");
        AppendLog(L"安装完成后请勿移动目录，否则需重新安装。");
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == IDC_DIR_BROWSE) {
            BROWSEINFOW bi = {};
            bi.hwndOwner = hWnd;
            bi.lpszTitle = L"选择安装目录";
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, path))
                    SetWindowTextW(g_hDirEdit, path);
                CoTaskMemFree(pidl);
            }
        }

        if (id == IDC_INSTALL_BTN && !g_installing) {
            wchar_t dir[MAX_PATH];
            GetWindowTextW(g_hDirEdit, dir, MAX_PATH);
            if (wcslen(dir) == 0) {
                MessageBoxW(hWnd, L"请先选择安装目录", L"提示", MB_OK | MB_ICONWARNING);
                break;
            }
            bool autoStart = SendMessageW(g_hAutoStartChk, BM_GETCHECK, 0, 0) == BST_CHECKED;
            bool useMirror = SendMessageW(g_hSrcMirror,    BM_GETCHECK, 0, 0) == BST_CHECKED;
            std::wstring confirm =
                std::wstring(L"安装目录：") + dir +
                L"\n开机启动：" + (autoStart ? L"是" : L"否") +
                L"\n下载源：" + (useMirror ? L"镜像源 (ghproxy)" : L"官方源 (GitHub)") +
                L"\n\n确认开始安装？";
            if (MessageBoxW(hWnd, confirm.c_str(), L"确认安装",
                            MB_YESNO | MB_ICONQUESTION) != IDYES) break;

            g_installing = true;
            SetWindowTextW(g_hLog, L"");
            AppendLog(L"开始安装...");

            InstallParams* p = new InstallParams{ std::wstring(dir), autoStart, useMirror };
            HANDLE hThread = CreateThread(NULL, 0, InstallThread, p, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }

        if (id == IDC_CANCEL_BTN) {
            if (g_installing &&
                MessageBoxW(hWnd, L"安装正在进行中，确定退出？",
                            L"确认", MB_YESNO | MB_ICONWARNING) != IDYES) break;
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
    CoInitialize(NULL);

    const wchar_t* CLS = L"WebWallpaperInstaller";
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLS;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(CLS, L"Web-Wallpaper 安装程序",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 510, 460,
        NULL, NULL, hInst, NULL);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }

    CoUninitialize();
    return 0;
}