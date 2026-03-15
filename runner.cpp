#include <windows.h>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

// 转换 string 到 wstring
wstring stringToWstring(const string& str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* buffer = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, len);
    wstring result(buffer);
    delete[] buffer;
    return result;
}

// 转换 wstring 到 string
string wstringToString(const wstring& wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    char* buffer = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, len, NULL, NULL);
    string result(buffer);
    delete[] buffer;
    return result;
}

string urlDecode(const string& str)
{
    string result;

    for (size_t i = 0; i < str.length(); i++)
    {
        if (str[i] == '%' && i + 2 < str.length())
        {
            string hex = str.substr(i + 1, 2);
            char ch = (char)strtol(hex.c_str(), nullptr, 16);

            result += ch;
            i += 2;
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }

    return result;
}

string extractCommand(string url)
{
    size_t pos = url.find("://");

    if (pos == string::npos)
        return "";

    string cmd = url.substr(pos + 3);

    if (!cmd.empty() && cmd.back() == '/')
        cmd.pop_back();

    return urlDecode(cmd);
}

// 执行命令（Unicode 版本）
bool executeCommand(const wstring& command, bool showWindow = true) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOW : SW_HIDE;

    ZeroMemory(&pi, sizeof(pi));

    // 执行命令
    bool success = CreateProcessW(
        NULL,                   // 应用程序名称
        (LPWSTR)command.c_str(), // 命令行
        NULL,                   // 进程安全属性
        NULL,                   // 线程安全属性
        FALSE,                  // 继承句柄
        0,                      // 创建标志
        NULL,                   // 环境变量
        NULL,                   // 当前目录
        &si,                    // 启动信息
        &pi                     // 进程信息
    );

// run 和 start 不等待（启动后台程序/打开文件）
    bool waitForProcess = !(command.substr(0, 3) == L"run" || command.substr(0, 5) == L"start");

    if (success) {
        if (waitForProcess) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return success;
}

int main(int argc, char* argv[]) {

    if (argc < 2)
        return 1;

    string command = extractCommand(argv[1]);

    if (command.empty())
        return 1;

    // 转换为宽字符
    wstring wCommand = stringToWstring(command);

    // 检查是否是 start 命令（用于打开文件）
    if (wCommand.substr(0, 5) == L"start") {
        // 直接执行 start 命令，不使用 cmd.exe /k
        // 移除 start 命令和可能的空引号
        wstring wStartCommand = wCommand;
        size_t startPos = wStartCommand.find(L"start");
        if (startPos != string::npos) {
            wStartCommand = wStartCommand.substr(startPos + 5);
            // 移除前导空格
            wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
            
            // 检查是否有引号
            if (!wStartCommand.empty() && wStartCommand[0] == L'"') {
                // 找到第一个引号的位置
                size_t firstQuote = wStartCommand.find(L'"');
                if (firstQuote != string::npos) {
                    // 找到第二个引号的位置
                    size_t secondQuote = wStartCommand.find(L'"', firstQuote + 1);
                    if (secondQuote != string::npos) {
                        // 移除空引号部分
                        wStartCommand = wStartCommand.substr(secondQuote + 1);
                        // 移除前导空格
                        wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                    }
                }
            }
            
            // 移除路径两边的引号
            if (!wStartCommand.empty() && wStartCommand[0] == L'"') {
                wStartCommand = wStartCommand.substr(1);
            }
            if (!wStartCommand.empty() && wStartCommand.back() == L'"') {
                wStartCommand.pop_back();
            }
        }
        
        // 使用 explorer.exe 打开文件，这样会使用系统默认应用
        wstring wRun = L"explorer.exe " + wStartCommand;
        executeCommand(wRun);
    } 
    // 检查是否是 run 命令（用于运行应用）
    else if (wCommand.substr(0, 3) == L"run") {
        // 移除 run 命令和可能的引号
        wstring wRunCommand = wCommand;
        size_t runPos = wRunCommand.find(L"run");
        if (runPos != string::npos) {
            wRunCommand = wRunCommand.substr(runPos + 3);
            // 移除前导空格
            wRunCommand = wRunCommand.substr(wRunCommand.find_first_not_of(L" \t"));
            
            // 移除路径两边的引号
            if (!wRunCommand.empty() && wRunCommand[0] == L'"') {
                wRunCommand = wRunCommand.substr(1);
            }
            if (!wRunCommand.empty() && wRunCommand.back() == L'"') {
                wRunCommand.pop_back();
            }
        }
        
        // 直接执行可执行文件
        executeCommand(wRunCommand);
    } else {
        // 其他命令使用 cmd.exe /k 执行
        wstring wRun = L"cmd.exe /k chcp 65001>nul && " + wCommand;
        executeCommand(wRun);
    }

    return 0;
}