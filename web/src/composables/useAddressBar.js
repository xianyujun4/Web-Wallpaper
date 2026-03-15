import { ref } from 'vue';
import { useGlobalState } from './useGlobalState';
import { useCommand } from './useCommand';
import { commandsConfig, loadCommands } from './useCommand';

// 添加历史记录
const addToHistory = (content, type) => {
  try {
    const history = JSON.parse(localStorage.getItem('history') || '[]');
    const now = new Date();
    const record = {
      id: Date.now(),
      date: now.toISOString().split('T')[0],
      time: now.toTimeString().split(' ')[0],
      content,
      type
    };
    history.unshift(record);
    // 限制历史记录数量为100条
    if (history.length > 100) {
      history.pop();
    }
    localStorage.setItem('history', JSON.stringify(history));
  } catch (error) {
    console.error('Failed to add to history:', error);
  }
};

// 地址栏输入状态
const addressInput = ref('');

// 全局状态和命令
const { currentEngine } = useGlobalState();
const { executeCommand } = useCommand();

// 判断是否是 URL（支持端口号和查询参数）
const isUrl = (str) => {
  // 支持端口号和查询参数的 URL 正则表达式
  const urlPattern = /^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})(:[\d]+)?([\/\w \.-]*)*(\?[\w\.\-\&\=\%\+]+)?\/?$/i;
  return urlPattern.test(str);
};

// 执行输入内容
const executeInput = async (input) => {
  const trimmedInput = input.trim();
  if (!trimmedInput) return;

  if (!commandsConfig.value) {
    await loadCommands();
  }

  // 自定义命令
  if (trimmedInput.startsWith('#')) {
    if (executeCommand(trimmedInput)) {
      addToHistory(trimmedInput, 'command');
      addressInput.value = '';
      return;
    }
  }

  // 终端命令
  if (trimmedInput.startsWith('$')) {

    const command = trimmedInput.substring(1).trim();

    if (command) {

      const encodedCommand = encodeURIComponent(command);

      // 使用iframe执行命令，避免页面跳转中断后续执行
      const iframe = document.createElement('iframe');
      iframe.style.display = 'none';
      iframe.src = 'mycmd://' + encodedCommand;
      document.body.appendChild(iframe);
      
      // 执行完成后移除iframe
      setTimeout(() => {
        document.body.removeChild(iframe);
      }, 100);
      
      addToHistory(trimmedInput, 'command');
    }

    addressInput.value = '';
    return;
  }

  // URL
  if (isUrl(trimmedInput)) {

    let url = trimmedInput;

    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      url = 'https://' + url;
    }

    window.open(url, '_blank');
    addToHistory(trimmedInput, 'url');

    addressInput.value = '';
    return;
  }

  // 搜索
  const searchUrls = {
    google: `https://www.google.com/search?q=${encodeURIComponent(trimmedInput)}`,
    bing: `https://www.bing.com/search?q=${encodeURIComponent(trimmedInput)}`,
    baidu: `https://www.baidu.com/s?wd=${encodeURIComponent(trimmedInput)}`
  };

  const searchUrl = searchUrls[currentEngine.value] || searchUrls.bing;

  window.open(searchUrl, '_blank');
  addToHistory(trimmedInput, 'search');

  addressInput.value = '';
};

export { addressInput, executeInput, isUrl };
