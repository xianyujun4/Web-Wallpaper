#include <windows.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#pragma comment(lib, "shell32.lib")

using namespace std;

// ── 编码转换 ──────────────────────────────────────────────────────────

wstring stringToWstring(const string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* buffer = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, len);
    wstring result(buffer);
    delete[] buffer;
    return result;
}

string wstringToString(const wstring& wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    char* buffer = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, len, NULL, NULL);
    string result(buffer);
    delete[] buffer;
    return result;
}

// ── URL 解码 ──────────────────────────────────────────────────────────

string urlDecode(const string& str) {
    string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            string hex = str.substr(i + 1, 2);
            char ch = (char)strtol(hex.c_str(), nullptr, 16);
            result += ch;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

// ── 从协议 URL 中提取命令字符串 ──────────────────────────────────────

string extractCommand(const string& url) {
    size_t pos = url.find("://");
    if (pos == string::npos) return "";

    string cmd = url.substr(pos + 3);

    // 移除末尾斜杠
    while (!cmd.empty() && cmd.back() == '/') cmd.pop_back();

    // 移除查询参数
    size_t queryPos = cmd.find('?');
    if (queryPos != string::npos) cmd = cmd.substr(0, queryPos);

    return urlDecode(cmd);
}

// ── 日志 ─────────────────────────────────────────────────────────────

static string g_logPath;

void logLine(const string& msg) {
    if (g_logPath.empty()) return;
    ofstream f(g_logPath.c_str(), ios::app);
    if (f.is_open()) f << msg << "\n";
}

void initLog(const string& logPath) {
    g_logPath = logPath;

    // 读取现有日志，只保留最近 100 条
    vector<string> entries;
    ifstream fin(logPath.c_str());
    if (fin.is_open()) {
        string line, cur;
        while (getline(fin, line)) {
            if (line == "=== New Execution ===") {
                if (!cur.empty()) { entries.push_back(cur); cur.clear(); }
                cur = line + "\n";
            } else {
                cur += line + "\n";
            }
        }
        if (!cur.empty()) entries.push_back(cur);
        fin.close();

        if (entries.size() > 100)
            entries.erase(entries.begin(), entries.begin() + (entries.size() - 100));

        ofstream fout(logPath.c_str(), ios::out);
        if (fout.is_open()) {
            for (auto& e : entries) fout << e;
        }
    }
}

// ── CreateProcess 封装（用于 cmd.exe 等） ────────────────────────────

bool runViaCreateProcess(const wstring& cmdline, bool wait = false) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
    ZeroMemory(&pi, sizeof(pi));

    wchar_t* buf = new wchar_t[cmdline.size() + 1];
    wcscpy_s(buf, cmdline.size() + 1, cmdline.c_str());

    bool ok = CreateProcessW(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (ok) {
        if (wait) WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    delete[] buf;

    logLine("CreateProcess: " + wstringToString(cmdline) + (ok ? " -> OK" : " -> FAILED"));
    return ok;
}

// ── ShellExecute 封装（用于打开文件/运行程序） ───────────────────────
// 比 explorer.exe 更可靠：正确处理带空格的路径、关联程序、UAC 等

bool runViaShellExecute(const wstring& path) {
    // 去掉两端残留引号（ShellExecuteW 不需要引号）
    wstring clean = path;
    if (!clean.empty() && clean.front() == L'"') clean.erase(clean.begin());
    if (!clean.empty() && clean.back()  == L'"') clean.pop_back();

    HINSTANCE result = ShellExecuteW(
        NULL,       // 父窗口
        L"open",    // 操作
        clean.c_str(), // 路径
        NULL,       // 参数
        NULL,       // 工作目录（ShellExecute 会自动从路径推断）
        SW_SHOW
    );

    // ShellExecuteW 返回值 > 32 表示成功
    bool ok = ((INT_PTR)result > 32);
    logLine("ShellExecute: " + wstringToString(clean) + (ok ? " -> OK" : " -> FAILED"));
    return ok;
}

// ── 单条命令分发 ──────────────────────────────────────────────────────
//
// 支持的命令格式（来自 JS 端 useCommand.js）：
//   start "" /d "C:\dir" "C:\file.exe"   → open 命令
//   start "" "C:\file.exe"               → open 命令（无 /d 参数时的兼容）
//   run "C:\file.exe"                    → run  命令
//   del/copy/move/rename ...             → cmd.exe /c 执行
//
// 关键设计：start / run 命令直接提取文件路径用 ShellExecuteW 打开，
// 其余命令交给 cmd.exe /c 执行，避免手动拼装 cmd 语法出错。

void dispatchCommand(const wstring& cmd) {
    logLine("Dispatch: " + wstringToString(cmd));

    // ── cmd /k：直接透传，保持窗口常驻，不加外层 cmd /c 包装 ───────────
  if (cmd.size() >= 7 && cmd.substr(0, 7) == L"cmd /k ") {
    logLine("  -> cmd /k passthrough: " + wstringToString(cmd));
    runViaCreateProcess(cmd, false);  // false = 不等待，窗口自己常驻
    return;
  }

  // ── start 命令：提取最后一个带引号的路径参数 ──────────────────────
    // 格式：start "" [/d "dir"] "filepath"
    // 无论有没有 /d 参数，文件路径始终是最后一个 "quoted" 段
    if (cmd.size() >= 5 && cmd.substr(0, 5) == L"start") {
        // 从末尾往前找最后一个完整的 "..." 段作为文件路径
        size_t endQuote = cmd.rfind(L'"');
        if (endQuote != wstring::npos && endQuote > 0) {
            size_t startQuote = cmd.rfind(L'"', endQuote - 1);
            if (startQuote != wstring::npos && startQuote < endQuote) {
                wstring filePath = cmd.substr(startQuote + 1, endQuote - startQuote - 1);
                logLine("  -> start path: " + wstringToString(filePath));
                runViaShellExecute(filePath);
                return;
            }
        }
        // 无引号兜底：把 start 后面的全部内容当路径
        wstring rest = cmd.substr(5);
        size_t nonspace = rest.find_first_not_of(L" \t");
        if (nonspace != wstring::npos) rest = rest.substr(nonspace);
        logLine("  -> start (no quotes) path: " + wstringToString(rest));
        runViaShellExecute(rest);
        return;
    }

    // ── run 命令：直接用 ShellExecuteW 启动可执行文件 ─────────────────
    if (cmd.size() >= 3 && cmd.substr(0, 3) == L"run") {
        wstring rest = cmd.substr(3);
        size_t nonspace = rest.find_first_not_of(L" \t");
        if (nonspace != wstring::npos) rest = rest.substr(nonspace);
        // 去掉两端引号
        if (!rest.empty() && rest.front() == L'"') rest.erase(rest.begin());
        if (!rest.empty() && rest.back()  == L'"') rest.pop_back();
        logLine("  -> run path: " + wstringToString(rest));
        runViaShellExecute(rest);
        return;
    }

    // ── 其他命令（del/copy/move/rename 等）：交给 cmd.exe /c ──────────
    // 用 /c 而非 /k，执行完自动关闭，不留悬挂窗口
    wstring cmdLine = L"cmd.exe /c chcp 65001>nul && " + cmd;
    runViaCreateProcess(cmdLine, true);
}

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // 初始化日志路径（exe 所在目录）
    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(NULL, exePathW, MAX_PATH);
    wstring exeDir(exePathW);
    size_t lastSep = exeDir.rfind(L'\\');
    if (lastSep != wstring::npos) exeDir = exeDir.substr(0, lastSep);
    string logPath = wstringToString(exeDir) + "\\runner.log";

    initLog(logPath);
    g_logPath = logPath;

    logLine("\n=== New Execution ===");
    logLine("argc: " + to_string(argc));
    for (int i = 0; i < argc; i++)
        logLine("argv[" + to_string(i) + "]: " + string(argv[i]));

    if (argc < 2) {
        logLine("Error: No argument provided");
        return 1;
    }

    // 提取并解码命令
    string command = extractCommand(argv[1]);
    logLine("Extracted command: " + command);

    if (command.empty()) {
        logLine("Error: Empty command after extraction");
        return 1;
    }

    wstring wCommand = stringToWstring(command);
    logLine("Wide command: " + wstringToString(wCommand));

    // 按 " & " 拆分多条命令，逐条分发
    // （JS 端多命令以 " & " 连接）
    size_t pos = 0;
    while (true) {
        size_t ampPos = wCommand.find(L" & ", pos);
        wstring singleCmd = (ampPos == wstring::npos)
            ? wCommand.substr(pos)
            : wCommand.substr(pos, ampPos - pos);

        if (!singleCmd.empty())
            dispatchCommand(singleCmd);

        if (ampPos == wstring::npos) break;
        pos = ampPos + 3;
    }

    // 记录结束时间
    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32];
    sprintf_s(ts, "Done: %04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);
    logLine(ts);

    return 0;
}