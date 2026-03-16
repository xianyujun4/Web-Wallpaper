#include <windows.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

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

    // 移除末尾的斜杠
    if (!cmd.empty() && cmd.back() == '/')
        cmd.pop_back();

    // 移除查询参数
    size_t queryPos = cmd.find('?');
    if (queryPos != string::npos)
        cmd = cmd.substr(0, queryPos);

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

    // 创建可修改的命令缓冲区
    wchar_t* commandBuffer = new wchar_t[command.length() + 1];
    wcscpy_s(commandBuffer, command.length() + 1, command.c_str());

    // 执行命令
    bool success = CreateProcessW(
        NULL,                   // 应用程序名称
        commandBuffer,           // 命令行（可修改缓冲区）
        NULL,                   // 进程安全属性
        NULL,                   // 线程安全属性
        FALSE,                  // 继承句柄
        0,                      // 创建标志
        NULL,                   // 环境变量
        NULL,                   // 当前目录
        &si,                    // 启动信息
        &pi                     // 进程信息
    );

// run、start 和 cmd.exe 不等待（启动后台程序/打开文件/打开命令行窗口）
    bool waitForProcess = !(command.substr(0, 3) == L"run" || command.substr(0, 5) == L"start" || command.substr(0, 7) == L"cmd.exe");

    if (success) {
        if (waitForProcess) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // 释放命令缓冲区
    delete[] commandBuffer;

    return success;
}

int main(int argc, char* argv[]) {
    // 获取当前目录（使用宽字符版本）
    wchar_t currentDirW[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDirW);
    
    // 转换为多字节字符
    char currentDir[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, currentDirW, -1, currentDir, MAX_PATH, NULL, NULL);
    
    // 构造日志文件路径
    std::string logPath = std::string(currentDir) + "\runner.log";
    
    // 读取现有日志，只保留最近的 100 条
    std::vector<std::string> logEntries;
    std::ifstream logFileIn(logPath.c_str());
    if (logFileIn.is_open()) {
        std::string line;
        std::string currentEntry;
        while (std::getline(logFileIn, line)) {
            if (line == "=== New Execution ===") {
                if (!currentEntry.empty()) {
                    logEntries.push_back(currentEntry);
                    currentEntry = "";
                }
                currentEntry = line + "\n";
            } else {
                currentEntry += line + "\n";
            }
        }
        if (!currentEntry.empty()) {
            logEntries.push_back(currentEntry);
        }
        logFileIn.close();
        
        // 只保留最近的 100 条日志
        if (logEntries.size() > 100) {
            logEntries.erase(logEntries.begin(), logEntries.begin() + (logEntries.size() - 100));
        }
        
        // 重写日志文件
        std::ofstream logFileOut(logPath.c_str(), std::ios::out);
        if (logFileOut.is_open()) {
            for (const auto& entry : logEntries) {
                logFileOut << entry;
            }
            logFileOut.close();
        }
    }
    
    // 打开日志文件（追加模式）
    std::ofstream logFile(logPath.c_str(), std::ios::app);
    
    // 检查文件是否打开成功
    if (!logFile.is_open()) {
        // 如果打开失败，尝试使用绝对路径（使用UTF-8编码）
        wchar_t absolutePathW[] = L"e:\\SomeTing\\Object\\Web-Wallpaper\\runner.log";
        char absolutePath[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, absolutePathW, -1, absolutePath, MAX_PATH, NULL, NULL);
        logPath = absolutePath;
        logFile.open(logPath.c_str(), std::ios::app);
    }
    
    // 声明变量
    string command;
    wstring wCommand;
    
    // 再次检查文件是否打开成功
    if (logFile.is_open()) {
        // 输出到日志文件
        logFile << "\n=== New Execution ===" << std::endl;
        logFile << "Current directory: " << currentDir << std::endl;
        logFile << "Log path: " << logPath << std::endl;
        logFile << "Full command line: " << GetCommandLineA() << std::endl;
        
        // 输出命令行参数
        logFile << "argc: " << argc << std::endl;
        for (int i = 0; i < argc; i++) {
            logFile << "argv[" << i << "]: " << argv[i] << std::endl;
        }

        if (argc < 2) {
            logFile << "Error: No command provided" << std::endl;
            logFile.close();
            return 1;
        }

        command = extractCommand(argv[1]);
        
        // 输出提取的命令
        logFile << "Extracted command: " << command << std::endl;

        if (command.empty()) {
            logFile << "Error: Empty command" << std::endl;
            logFile.close();
            return 1;
        }

        // 转换为宽字符
        wCommand = stringToWstring(command);
        
        // 输出宽字符命令
        logFile << "Wide command: " << wstringToString(wCommand) << std::endl;
        
        // 关闭日志文件
        logFile.close();
    } else {
        // 如果无法打开日志文件，尝试使用消息框显示信息
        std::string message = "无法打开日志文件: " + logPath;
        MessageBoxA(NULL, message.c_str(), "Error", MB_OK);
        
        // 即使无法打开日志文件，也要继续执行命令
        if (argc < 2) {
            return 1;
        }
        
        command = extractCommand(argv[1]);
        
        if (command.empty()) {
            return 1;
        }
        
        // 转换为宽字符
        wCommand = stringToWstring(command);
    }

    // 检查是否包含多个命令（使用 & 分隔）
    size_t ampPos = wCommand.find(L" & ");
    if (ampPos != string::npos) {
        // 分割命令
        wstring firstCommand = wCommand.substr(0, ampPos);
        wstring restCommand = wCommand.substr(ampPos + 3);
        
        // 处理第一个命令
        if (firstCommand.substr(0, 5) == L"start") {
            // 直接执行 start 命令，不使用 cmd.exe /k
            // 移除 start 命令和可能的空引号
            wstring wStartCommand = firstCommand;
            size_t startPos = wStartCommand.find(L"start");
            if (startPos != string::npos) {
                wStartCommand = wStartCommand.substr(startPos + 5);
                // 移除前导空格
                wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                
                // 处理 start 命令的参数
                // 跳过空引号
                if (!wStartCommand.empty() && wStartCommand[0] == L'"') {
                    // 找到第一个引号的位置
                    size_t firstQuote = wStartCommand.find(L'"');
                    if (firstQuote != string::npos) {
                        // 找到第二个引号的位置
                        size_t secondQuote = wStartCommand.find(L'"', firstQuote + 1);
                        if (secondQuote != string::npos) {
                            // 跳过空引号部分
                            wStartCommand = wStartCommand.substr(secondQuote + 1);
                            // 移除前导空格
                            wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                        }
                    }
                }
                
                // 提取文件路径（保留引号）
                wstring filePath = wStartCommand;
                
                // 如果文件路径被引号包围，移除引号
                if (!filePath.empty() && filePath[0] == L'"') {
                    filePath = filePath.substr(1);
                }
                if (!filePath.empty() && filePath.back() == L'"') {
                    filePath.pop_back();
                }
                
                // 使用 explorer.exe 打开文件，这样会使用系统默认应用
                wstring wRun = L"explorer.exe " + filePath;
                executeCommand(wRun);
            }
        } 
        // 检查是否是 run 命令（用于运行应用）
        else if (firstCommand.substr(0, 3) == L"run") {
            // 移除 run 命令和可能的引号
            wstring wRunCommand = firstCommand;
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
            wstring wRun = L"cmd.exe /k chcp 65001>nul && " + firstCommand;
            executeCommand(wRun);
        }
        
        // 处理剩余的命令
        // 检查剩余命令是否包含 start 或 run 命令
        if (restCommand.substr(0, 5) == L"start") {
            // 直接执行 start 命令，不使用 cmd.exe /k
            // 移除 start 命令和可能的空引号
            wstring wStartCommand = restCommand;
            size_t startPos = wStartCommand.find(L"start");
            if (startPos != string::npos) {
                wStartCommand = wStartCommand.substr(startPos + 5);
                // 移除前导空格
                wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                
                // 处理 start 命令的参数
                // 跳过空引号
                if (!wStartCommand.empty() && wStartCommand[0] == L'"') {
                    // 找到第一个引号的位置
                    size_t firstQuote = wStartCommand.find(L'"');
                    if (firstQuote != string::npos) {
                        // 找到第二个引号的位置
                        size_t secondQuote = wStartCommand.find(L'"', firstQuote + 1);
                        if (secondQuote != string::npos) {
                            // 跳过空引号部分
                            wStartCommand = wStartCommand.substr(secondQuote + 1);
                            // 移除前导空格
                            wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                        }
                    }
                }
                
                // 提取文件路径（保留引号）
                wstring filePath = wStartCommand;
                
                // 如果文件路径被引号包围，移除引号
                if (!filePath.empty() && filePath[0] == L'"') {
                    filePath = filePath.substr(1);
                }
                if (!filePath.empty() && filePath.back() == L'"') {
                    filePath.pop_back();
                }
                
                // 使用 explorer.exe 打开文件，这样会使用系统默认应用
                wstring wRun = L"explorer.exe " + filePath;
                executeCommand(wRun);
            }
        } else if (restCommand.substr(0, 3) == L"run") {
            // 移除 run 命令和可能的引号
            wstring wRunCommand = restCommand;
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
            wstring wRun = L"cmd.exe /k chcp 65001>nul && " + restCommand;
            executeCommand(wRun);
        }
    } else {
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
                
                // 处理 start 命令的参数
                // 跳过空引号
                if (!wStartCommand.empty() && wStartCommand[0] == L'"') {
                    // 找到第一个引号的位置
                    size_t firstQuote = wStartCommand.find(L'"');
                    if (firstQuote != string::npos) {
                        // 找到第二个引号的位置
                        size_t secondQuote = wStartCommand.find(L'"', firstQuote + 1);
                        if (secondQuote != string::npos) {
                            // 跳过空引号部分
                            wStartCommand = wStartCommand.substr(secondQuote + 1);
                            // 移除前导空格
                            wStartCommand = wStartCommand.substr(wStartCommand.find_first_not_of(L" \t"));
                        }
                    }
                }
                
                // 提取文件路径（保留引号）
                wstring filePath = wStartCommand;
                
                // 如果文件路径被引号包围，移除引号
                if (!filePath.empty() && filePath[0] == L'"') {
                    filePath = filePath.substr(1);
                }
                if (!filePath.empty() && filePath.back() == L'"') {
                    filePath.pop_back();
                }
                
                // 使用 explorer.exe 打开文件，这样会使用系统默认应用
                wstring wRun = L"explorer.exe " + filePath;
                executeCommand(wRun);
            }
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
    }

    // 添加事件时间戳到日志文件
    std::ofstream logFileAppend(logPath.c_str(), std::ios::app);
    if (logFileAppend.is_open()) {
        // 获取当前时间
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeStr[20];
        sprintf_s(timeStr, "%04d-%02d-%02d_%02d:%02d:%02d", 
                  st.wYear, st.wMonth, st.wDay, 
                  st.wHour, st.wMinute, st.wSecond);
        logFileAppend << "Event: " << timeStr << std::endl;
        logFileAppend.close();
    }

    return 0;
}