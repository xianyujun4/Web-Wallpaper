// 自定义命令功能模块
import { ref } from 'vue';

const commandsConfig = ref(null);

// ── 自定义指令统一从 localStorage 读写，是唯一数据源 ──────────────────
// 所有模块共享同一个 ref，避免各自维护独立副本导致不同步
const customCommands = ref([]);

// 默认命令配置
const defaultCommands = {
  commands: {
    open: {
      description: "打开文件或文件夹",
      type: "system",
      execute: 'start "" "{0}"'
    },
    run: {
      description: "运行应用",
      type: "system",
      execute: 'start "" "{0}"'
    },
    del: {
      description: "删除文件",
      type: "system",
      execute: 'del "{0}"'
    },
    copy: {
      description: "复制文件",
      type: "system",
      execute: 'copy "{0}" "{1}"'
    },
    move: {
      description: "移动文件",
      type: "system",
      execute: 'move "{0}" "{1}"'
    },
    rename: {
      description: "重命名文件",
      type: "system",
      execute: 'rename "{0}" "{1}"'
    }
  }
};

// ── localStorage 读写工具 ──────────────────────────────────────────────

/**
 * 从 localStorage 读取自定义指令列表
 * @returns {Array} 指令数组
 */
const readCommandsFromStorage = () => {
  try {
    const stored = localStorage.getItem('customCommands');
    if (stored) {
      const data = JSON.parse(stored);
      return Array.isArray(data.commands) ? data.commands : [];
    }
  } catch (e) {
    console.error('读取自定义指令失败:', e);
  }
  return [];
};

/**
 * 将自定义指令列表写入 localStorage，并同步到内存 ref
 * @param {Array} commands
 */
const writeCommandsToStorage = (commands) => {
  try {
    localStorage.setItem('customCommands', JSON.stringify({ commands }));
    customCommands.value = commands;
  } catch (e) {
    console.error('保存自定义指令失败:', e);
  }
};

// ── 加载配置 ──────────────────────────────────────────────────────────

// 加载 command.json 系统命令配置
const loadCommands = async () => {
  try {
    const response = await fetch('/command.json?t=' + Date.now());
    const data = await response.json();
    commandsConfig.value = data;
  } catch (error) {
    console.warn('command.json 加载失败，使用默认命令');
    commandsConfig.value = defaultCommands;
  }

  // 从 localStorage 同步自定义指令到内存
  customCommands.value = readCommandsFromStorage();
};

// ── 路径工具 ──────────────────────────────────────────────────────────

/**
 * 规范化路径：统一转为反斜杠，去掉多余引号
 */
const normalizePath = (raw) => {
  return raw.replace(/"/g, '').replace(/\//g, '\\');
};

/**
 * 从完整路径提取目录部分（给 start /d 用）
 * 例：D:\apps\foo.exe → D:\apps
 */
const getDirFromPath = (fullPath) => {
  const normalized = normalizePath(fullPath);
  const lastSep = normalized.lastIndexOf('\\');
  return lastSep > 0 ? normalized.substring(0, lastSep) : normalized;
};

// ── 输入类型判断 ──────────────────────────────────────────────────────

/**
 * 判断一行输入的类型：
 *   'hash'  → # 开头，走系统命令 / 自定义指令
 *   'shell' → $ 开头，走 cmd.exe 执行 shell 命令
 *   'url'   → http(s):// 开头或 xxx.xxx 形式，直接在浏览器打开
 *   null    → 无法识别，忽略
 */
const detectLineType = (line) => {
  const t = line.trim();
  if (!t) return null;
  if (t.startsWith('#')) return 'hash';
  if (t.startsWith('$')) return 'shell';
  if (/^https?:\/\//i.test(t)) return 'url';
  // 宽松匹配：包含 . 且没有空格，视为网址（补全 https://）
  if (/^[^\s]+\.[^\s]+$/.test(t) && !t.startsWith('/') && !t.match(/^[A-Za-z]:\\/)) return 'url';
  return null;
};

// ── 命令解析（# 指令专用） ────────────────────────────────────────────

/**
 * 解析 "#命令名 参数1 参数2" 格式
 * 支持带引号的参数（内部空格）
 */
const parseCommand = (input) => {
  const trimmed = input.replace(/[\r\n]+/g, '').trim();
  if (!trimmed.startsWith('#')) return null;

  const commandStr = trimmed.substring(1).trim();
  if (!commandStr) return null;

  const parts = [];
  let current = '';
  let inQuotes = false;

  for (let i = 0; i < commandStr.length; i++) {
    const char = commandStr[i];
    if (char === '"') {
      inQuotes = !inQuotes;
      continue;
    }
    if (char === ' ' && !inQuotes) {
      if (current.length > 0) { parts.push(current); current = ''; }
    } else {
      current += char;
    }
  }
  if (current.length > 0) parts.push(current);
  if (parts.length === 0) return null;

  return {
    commandName: parts[0].toLowerCase(),
    args: parts.slice(1).map(p => p.replace(/[\r\n]+/g, '').trim())
  };
};

// ── 展开单行输入为系统命令字符串 ──────────────────────────────────────

/**
 * 将一行输入展开为 runner.cpp 能执行的命令字符串数组。
 * # 指令  → start "" "路径" 等
 * $ 命令  → cmd /c xxx（直接透传给 shell）
 * URL     → start "" "https://..." （用默认浏览器打开）
 */
const expandLine = (line, depth = 0) => {
  if (depth > 10) {
    console.warn('指令展开深度超限:', line);
    return [];
  }

  const t = line.trim();
  const type = detectLineType(t);

  // ── URL：直接用浏览器打开，不走 runner.cpp ────────────────────────
  if (type === 'url') {
    const url = /^https?:\/\//i.test(t) ? t : 'https://' + t;
    console.log('识别为 URL，直接打开:', url);
    window.open(url, '_blank');
    return [];  // 不产生系统命令，已经处理完毕
  }

  // ── $ shell 命令：去掉 $ 前缀，交给 cmd /c 执行 ───────────────────
  if (type === 'shell') {
    const shellCmd = t.substring(1).trim();
    console.log('识别为 shell 命令:', shellCmd);
    // cmd /k 保持窗口显示结果；如不需要留窗口改为 /c
    return [`cmd /k ${shellCmd}`];
  }

  // ── # 指令：走原有解析逻辑 ────────────────────────────────────────
  if (type === 'hash') {
    const parsed = parseCommand(t);
    if (!parsed) return [];

    const { commandName, args } = parsed;

    // 自定义指令集：递归展开每一行
    const customCommand = customCommands.value.find(
      cmd => cmd.name.toLowerCase() === commandName
    );
    if (customCommand) {
      const subLines = customCommand.commands
        .split(/\\n|\n|\r\n/)
        .map(l => l.trim())
        .filter(l => l.length > 0);

      const result = [];
      subLines.forEach(sub => result.push(...expandLine(sub, depth + 1)));
      return result;
    }

    // 系统命令：替换占位符
    const command =
      commandsConfig.value?.commands?.[commandName] ||
      defaultCommands.commands?.[commandName];

    if (!command) {
      console.warn(`未找到命令: ${commandName}`);
      return [];
    }

    let executeStr = command.execute;
    args.forEach((arg, index) => {
      const normalized = normalizePath(arg);
      executeStr = executeStr.replaceAll(`{${index}}`, normalized);
      executeStr = executeStr.replaceAll(`{dir${index}}`, getDirFromPath(normalized));
    });
    return [executeStr];
  }

  console.warn('无法识别的输入行:', t);
  return [];
};

// ── 协议触发 ──────────────────────────────────────────────────────────

/**
 * 通过 <a> 标签触发 mycmd:// 协议（浏览器视为用户手势，不拦截）
 * 多条命令用 " & " 拼接，runner.cpp 负责按顺序执行
 */
const triggerProtocol = (systemCmds) => {
  if (systemCmds.length === 0) return;

  const joined = systemCmds.join(' & ');
  console.log('触发协议命令:', joined);

  const a = document.createElement('a');
  a.href = 'mycmd://' + encodeURIComponent(joined);
  a.style.display = 'none';
  document.body.appendChild(a);
  a.click();
  setTimeout(() => document.body.removeChild(a), 500);
};

// ── 对外接口 ──────────────────────────────────────────────────────────

/**
 * 执行一行输入（# 指令 / $ shell / URL 均可）
 * 也可以是多行（自定义指令集内容），逐行解析后合并一次触发
 * @returns {boolean} 是否成功触发
 */
const executeCommand = (input) => {
  // 兜底：loadCommands 可能还没跑，先从 localStorage 补充
  if (customCommands.value.length === 0) {
    customCommands.value = readCommandsFromStorage();
  }

  const t = input.trim();

  // 如果输入本身就是 URL，直接打开，不走 runner.cpp
  const directType = detectLineType(t);
  if (directType === 'url') {
    const url = /^https?:\/\//i.test(t) ? t : 'https://' + t;
    console.log('直接打开 URL:', url);
    window.open(url, '_blank');
    return true;
  }

  const systemCmds = expandLine(input);
  if (systemCmds.length === 0) return false;

  triggerProtocol(systemCmds);
  return true;
};

// ── 公开 API ──────────────────────────────────────────────────────────

export { commandsConfig, customCommands, loadCommands, readCommandsFromStorage, writeCommandsToStorage };

export function useCommand() {
  return {
    executeCommand,
    loadCommands,
    customCommands,
    readCommandsFromStorage,
    writeCommandsToStorage
  };
}