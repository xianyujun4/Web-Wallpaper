// 自定义命令功能模块
import { ref } from 'vue';

const commandsConfig = ref(null);

// 默认命令配置
const defaultCommands = {
  commands: {
    open: {
      description: "打开文件",
      type: "system",
      execute: "start \"\" \"{0}\""
    },
    run: {
      description: "运行应用",
      type: "system",
      execute: "run \"{0}\""
    },
    del: {
      description: "删除文件",
      type: "system",
      execute: 'del {0}'
    },
    copy: {
      description: "复制文件",
      type: "system",
      execute: 'copy {0} {1}'
    },
    move: {
      description: "移动文件",
      type: "system",
      execute: 'move {0} {1}'
    },
    rename: {
      description: "重命名文件",
      type: "system",
      execute: 'rename {0} {1}'
    }
  }
};

// 加载命令配置
const loadCommands = async () => {
  try {
    const response = await fetch('/command.json?t=' + Date.now());
    const data = await response.json();
    commandsConfig.value = data;
  } catch (error) {
    console.warn('command.json 加载失败，使用默认命令');
    commandsConfig.value = defaultCommands;
  }
};

// 解析命令
const parseCommand = (input) => {
  if (!input.startsWith('#')) return null;

  const commandStr = input.substring(1).trim();

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
      if (current.length > 0) {
        parts.push(current);
        current = '';
      }
    } else {
      current += char;
    }
  }

  if (current.length > 0) {
    parts.push(current);
  }

  if (parts.length === 0) return null;

  const commandName = parts[0].toLowerCase();
  const args = parts.slice(1);

  return { commandName, args };
};

// 执行命令
const executeCommand = (input) => {
  const parsed = parseCommand(input);
  if (!parsed) return false;

  const { commandName, args } = parsed;

  const command =
    commandsConfig.value?.commands?.[commandName] ||
    defaultCommands.commands?.[commandName];

  if (!command) return false;

  let executeStr = command.execute;

  args.forEach((arg, index) => {
    let safeArg = arg.replace(/\//g, '\\');
    if (!safeArg.startsWith('"')) {
      safeArg = `"${safeArg}"`;
    }
    executeStr = executeStr.replace(`{${index}}`, safeArg);
  });

  console.log("执行命令:", executeStr);

  const encodedCommand = encodeURIComponent(executeStr);

  // 在 executeCommand 里，iframe 那段改成这样
  const iframe = document.createElement('iframe');
  iframe.style.display = 'none';
  iframe.src = 'mycmd://' + encodedCommand;
  console.log('iframe src 设置为:', iframe.src);
  document.body.appendChild(iframe);
  setTimeout(() => document.body.removeChild(iframe), 1000);

  return true;
};

export { commandsConfig, loadCommands };

export function useCommand() {
  return {
    executeCommand,
    loadCommands
  };
}