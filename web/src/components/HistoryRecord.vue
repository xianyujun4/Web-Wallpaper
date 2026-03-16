<!-- 历史记录组件 -->
<script setup>
import { ref, computed, onMounted } from 'vue';
import { executeInput } from '../composables/useAddressBar';
import { useGlobalState } from '../composables/useGlobalState';

// 全局状态
const { currentEngine } = useGlobalState();

// 声明组件可以发射的事件
const emit = defineEmits(['close', 'go-to-home']);

// 历史记录数据
const historyRecords = ref([]);
// 搜索关键词
const searchKeyword = ref('');
// 日期搜索关键词
const dateKeyword = ref('');
// 选中的历史记录
const selectedRecords = ref([]);

// 加载历史记录
const loadHistory = () => {
  try {
    const savedHistory = localStorage.getItem('history');
    if (savedHistory) {
      let history = JSON.parse(savedHistory);
      // 检查并修复重复的id
      const idSet = new Set();
      history = history.map(record => {
        // 如果id重复或不是数字，生成新的时间戳id
        if (!record.id || idSet.has(record.id)) {
          const newId = Date.now() - Math.floor(Math.random() * 10000);
          idSet.add(newId);
          return { ...record, id: newId };
        }
        idSet.add(record.id);
        return record;
      });
      historyRecords.value = history;
      // 保存修复后的历史记录
      localStorage.setItem('history', JSON.stringify(historyRecords.value));
    } else {
      // 模拟历史记录数据
      historyRecords.value = [
        {
          id: Date.now() - 10000,
          date: '2026-03-15',
          time: '19:49:06',
          content: 'https://www.google.com',
          type: 'url'
        },
        {
          id: Date.now() - 9000,
          date: '2026-03-15',
          time: '19:48:30',
          content: '#open D:\test.txt',
          type: 'command'
        },
        {
          id: Date.now() - 8000,
          date: '2026-03-15',
          time: '19:47:15',
          content: 'github',
          type: 'search'
        },
        {
          id: Date.now() - 7000,
          date: '2026-03-14',
          time: '18:30:00',
          content: 'https://www.baidu.com',
          type: 'url'
        },
        {
          id: Date.now() - 6000,
          date: '2026-03-14',
          time: '18:25:00',
          content: '$ipconfig',
          type: 'command'
        }
      ];
      localStorage.setItem('history', JSON.stringify(historyRecords.value));
    }
  } catch (error) {
    console.error('Failed to load history:', error);
    historyRecords.value = [];
  }
};

// 按日期分组历史记录
const groupedHistory = computed(() => {
  const groups = {};
  historyRecords.value.forEach(record => {
    if (!groups[record.date]) {
      groups[record.date] = [];
    }
    groups[record.date].push(record);
  });
  return groups;
});

// 过滤后的历史记录
const filteredHistory = computed(() => {
  const filtered = {};
  Object.keys(groupedHistory.value).forEach(date => {
    // 日期搜索
    const dateMatch = !dateKeyword.value || 
      date.includes(dateKeyword.value);
    
    if (dateMatch) {
      const records = groupedHistory.value[date].filter(record => {
        // 内容搜索
        return !searchKeyword.value || 
          record.content.toLowerCase().includes(searchKeyword.value.toLowerCase());
      });
      if (records.length > 0) {
        filtered[date] = records;
      }
    }
  });
  return filtered;
});

// 处理历史记录点击
const handleRecordClick = (event, record) => {
  event.stopPropagation(); // 阻止事件冒泡
  const index = selectedRecords.value.indexOf(record.id);
  if (index === -1) {
    selectedRecords.value.push(record.id);
  } else {
    selectedRecords.value.splice(index, 1);
  }
};

// 处理删除选中的历史记录
const handleDeleteSelected = () => {
  historyRecords.value = historyRecords.value.filter(record => 
    !selectedRecords.value.includes(record.id)
  );
  selectedRecords.value = [];
  // 保存到本地存储
  localStorage.setItem('history', JSON.stringify(historyRecords.value));
};

// 解析输入内容类型
function parseInput(content) {
  if (content.startsWith('$')) {
    return { type: 'shell', value: content.slice(1) };
  }

  if (content.startsWith('#')) {
    return { type: 'custom', value: content.slice(1) };
  }

  if (/^https?:\/\//i.test(content)) {
    return { type: 'url', value: content };
  }

  return { type: 'search', value: content };
}

// 处理前往选中的历史记录

// 辅助函数：使用隐藏 iframe 触发本地协议，避免使用 window.location 导致当前页面卸载或任务中断
const triggerLocalProtocol = (url) => {
  const iframe = document.createElement('iframe');
  iframe.style.display = 'none';
  iframe.src = url;
  document.body.appendChild(iframe);
  // 触发后清理 DOM
  setTimeout(() => {
    document.body.removeChild(iframe);
  }, 1000);
};

// 处理前往选中的历史记录
const handleGoToSelected = async () => {
  console.log('selectedRecords:', selectedRecords.value);
  const selected = historyRecords.value.filter(record => 
    selectedRecords.value.includes(record.id)
  );
  console.log('selected records:', selected);

  if (selected.length === 0) return;

  const urls = [];
  const shellCommands = [];
  const customCommands = [];
  let isPopupBlocked = false;

  selected.forEach(r => {
    const result = parseInput(r.content);
    console.log('parsed record:', r.content, result);
    if (result.type === 'url' || result.type === 'search') {
      urls.push(r.content);
    } else if (result.type === 'shell') {
      shellCommands.push(result.value);
    } else if (result.type === 'custom') {
      customCommands.push(result.value);
    }
  });
  console.log('shellCommands:', shellCommands);

  // 处理网页和搜索（不变）
  urls.forEach(content => {
    const result = parseInput(content);
    let targetUrl;
    if (result.type === 'url') {
      targetUrl = result.value;
    } else {
      const searchUrls = {
        google: `https://www.google.com/search?q=${encodeURIComponent(result.value)}`,
        bing: `https://www.bing.com/search?q=${encodeURIComponent(result.value)}`,
        baidu: `https://www.baidu.com/s?wd=${encodeURIComponent(result.value)}`
      };
      targetUrl = searchUrls[currentEngine.value] || searchUrls.bing;
    }
    const newWindow = window.open(targetUrl, '_blank');
    if (!newWindow) isPopupBlocked = true;
  });

  // 处理所有命令（$ 和 # 指令）：将它们合并为一个，使用 & 连接，只执行一次
  const allCommands = [];
  
  // 添加 $ 指令
  if (shellCommands.length > 0) {
    console.log('adding shell commands:', shellCommands);
    // 确保每个命令末尾没有斜杠
    const cleanShellCommands = shellCommands.map(cmd => cmd.replace(/\/$/, ''));
    allCommands.push(...cleanShellCommands);
  }
  
  // 添加 # 指令
  if (customCommands.length > 0) {
    console.log('adding custom commands:', customCommands);
    // 确保每个命令末尾没有斜杠，并且将 open 指令转换为 start 指令
    const cleanCustomCommands = customCommands.map(cmd => {
      const cleanCmd = cmd.replace(/\/$/, '');
      // 检查是否是 open 指令
      if (cleanCmd.startsWith('open ')) {
        // 将 open 指令转换为 start 指令
        const filePath = cleanCmd.substring(5);
        // 将 "/" 替换为 "\\"
        const safeFilePath = filePath.replace(/\//g, '\\');
        return 'start "" "' + safeFilePath + '"';
      }
      return cleanCmd;
    });
    allCommands.push(...cleanCustomCommands);
  }
  
  // 执行所有命令
  if (allCommands.length > 0) {
    console.log('executing all commands:', allCommands);
    // 将所有命令合并为一个，使用 & 连接
    const combinedCommand = allCommands.join(' & ');
    console.log('combined all command:', combinedCommand);
    const iframe = document.createElement('iframe');
    iframe.style.display = 'none';
    iframe.src = 'mycmd://' + encodeURIComponent(combinedCommand);
    document.body.appendChild(iframe);
    console.log('iframe created with src:', iframe.src);
    // 立即移除 iframe，因为命令已经触发
    setTimeout(() => {
      document.body.removeChild(iframe);
      console.log('iframe removed for combined all command');
    }, 100);
  }

  if (isPopupBlocked) {
    alert('部分网页被浏览器拦截了！\n请点击浏览器地址栏右侧的🚫图标，选择"始终允许弹出式窗口"。');
  }

  emit('go-to-home');
};

// 处理关闭窗口
const handleClose = () => {
  emit('close');
};

// 组件挂载时加载历史记录
onMounted(() => {
  loadHistory();
});
</script>

<template>
  <div class="history-overlay" @click.stop>
    <div class="history-window">
      <div class="history-header">
        <h3>历史记录</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>
      
      <!-- 历史记录搜索框 -->
      <div class="history-search">
        <div class="search-container">
          <input 
            type="text" 
            v-model="searchKeyword" 
            placeholder="搜索历史记录"
            class="search-input"
            @click.stop
          />
          <input 
            type="text" 
            v-model="dateKeyword" 
            placeholder="搜索日期 (YYYY-MM-DD)"
            class="search-input date-search"
            @click.stop
          />
        </div>
      </div>
      
      <!-- 历史记录内容 -->
      <div class="history-content">
        <div v-if="Object.keys(filteredHistory).length === 0" class="empty-history">
          没有历史记录
        </div>
        <div v-else>
          <div 
            v-for="(records, date) in filteredHistory" 
            :key="date"
            class="history-group"
          >
            <div class="date-header">{{ date }}</div>
            <div 
              v-for="record in records" 
              :key="record.id"
              class="history-item"
              :class="{ 'selected': selectedRecords.includes(record.id) }"
              @click="handleRecordClick($event, record)"
            >
              <div class="record-time">{{ record.time }}</div>
              <div class="record-content">{{ record.content }}</div>
              <div class="record-type" :class="record.type">{{ record.type === 'url' ? '网页' : record.type === 'command' ? '命令' : '搜索' }}</div>
            </div>
          </div>
        </div>
      </div>
      
      <!-- 底部按钮 -->
      <div class="history-footer">
        <button class="delete-button" @click.stop="handleDeleteSelected">删除</button>
        <button class="go-button" @click.stop="handleGoToSelected">
          <div>前往</div>
          <div class="go-button-note">目前还有BUG，只能打开多网页，无法执行多命令</div>
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 历史记录窗口覆盖层 */
.history-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: rgba(0, 0, 0, 0.3);
  backdrop-filter: blur(5px);
  -webkit-backdrop-filter: blur(5px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 9999;
  opacity: 1;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  pointer-events: auto;
}

/* 历史记录窗口 */
.history-window {
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  padding: 20px;
  width: 90%;
  max-width: 600px;
  height: 80%;
  max-height: 600px;
  color: rgba(255, 255, 255, 0.9);
  margin: auto;
  position: relative;
  z-index: 10000;
  pointer-events: auto;
  display: flex;
  flex-direction: column;
}

/* 历史记录窗口头部 */
.history-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.history-header h3 {
  margin: 0;
  font-size: 18px;
  font-weight: 500;
}

.close-button {
  background: none;
  border: none;
  color: rgba(255, 255, 255, 0.7);
  font-size: 24px;
  cursor: pointer;
  padding: 0;
  width: 30px;
  height: 30px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 50%;
  transition: all 0.2s ease;
}

.close-button:hover {
  background-color: rgba(255, 255, 255, 0.1);
  color: rgba(255, 255, 255, 0.9);
}

/* 历史记录搜索框 */
.history-search {
  margin-bottom: 15px;
  position: sticky;
  top: 0;
  z-index: 10;
  background-color: rgba(0, 0, 0, 0.8);
  padding: 10px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.search-container {
  display: flex;
  gap: 10px;
}

.search-input {
  flex: 1;
  padding: 10px 15px;
  background-color: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 20px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 14px;
  outline: none;
  box-sizing: border-box;
}

.search-input.date-search {
  flex: 0 0 150px;
}

.search-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

/* 历史记录内容 */
.history-content {
  flex: 1;
  overflow-y: auto;
  margin-bottom: 15px;
}

/* 空历史记录 */
.empty-history {
  text-align: center;
  padding: 40px 0;
  color: rgba(255, 255, 255, 0.5);
  font-size: 14px;
}

/* 历史记录分组 */
.history-group {
  margin-bottom: 20px;
}

/* 日期标题 */
.date-header {
  font-size: 14px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.7);
  margin-bottom: 10px;
  padding-left: 10px;
  border-left: 3px solid rgba(255, 255, 255, 0.3);
}

/* 历史记录项 */
.history-item {
  display: flex;
  align-items: center;
  padding: 12px 15px;
  cursor: pointer;
  transition: all 0.2s ease;
  position: relative;
}

.history-item::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 1px;
  background: linear-gradient(to right, transparent, rgba(255, 255, 255, 0.1), transparent);
}

.history-item::after {
  content: '';
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 1px;
  background: linear-gradient(to right, transparent, rgba(255, 255, 255, 0.1), transparent);
}

.history-item:hover {
  background-color: rgba(255, 255, 255, 0.05);
}

.history-item.selected {
  background-color: rgba(255, 255, 255, 0.1);
  border-left: 3px solid rgba(255, 255, 255, 0.5);
}

/* 记录时间 */
.record-time {
  width: 80px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.5);
  flex-shrink: 0;
}

/* 记录内容 */
.record-content {
  flex: 1;
  font-size: 14px;
  margin: 0 15px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* 记录类型 */
.record-type {
  font-size: 12px;
  padding: 2px 8px;
  border-radius: 10px;
  flex-shrink: 0;
}

.record-type.url {
  background-color: rgba(33, 150, 243, 0.2);
  color: rgba(33, 150, 243, 0.9);
}

.record-type.command {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

.record-type.search {
  background-color: rgba(255, 152, 0, 0.2);
  color: rgba(255, 152, 0, 0.9);
}

/* 历史记录窗口底部 */
.history-footer {
  display: flex;
  justify-content: space-between;
  padding-top: 15px;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  position: sticky;
  bottom: 0;
  background-color: rgba(0, 0, 0, 0.8);
}

/* 底部按钮 */
.delete-button, .go-button {
  padding: 10px 20px;
  border: none;
  border-radius: 6px;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
  flex: 1;
  margin: 0 5px;
}

.delete-button {
  background-color: rgba(255, 87, 34, 0.2);
  color: rgba(255, 87, 34, 0.9);
  display: flex;
  align-items: center;
  justify-content: center;
}

.delete-button:hover {
  background-color: rgba(255, 87, 34, 0.3);
  color: rgba(255, 87, 34, 1);
}

.go-button {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

.go-button:hover {
  background-color: rgba(76, 175, 80, 0.3);
  color: rgba(76, 175, 80, 1);
}

/* 前往按钮的说明文字 */
.go-button {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}

.go-button-note {
  font-size: 10px;
  color: rgba(76, 175, 80, 0.7);
  margin-top: 4px;
  text-align: center;
  line-height: 1.2;
}

/* 滚动条样式 */
.history-content::-webkit-scrollbar {
  width: 8px;
}

.history-content::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
}

.history-content::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.2);
  border-radius: 4px;
}

.history-content::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.3);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .history-window {
    width: 95%;
    height: 85%;
    padding: 15px;
  }
  
  .search-container {
    flex-direction: column;
  }
  
  .search-input.date-search {
    flex: 1;
  }
  
  .record-time {
    width: 70px;
    font-size: 11px;
  }
  
  .record-content {
    font-size: 13px;
    margin: 0 10px;
  }
  
  .record-type {
    font-size: 11px;
    padding: 2px 6px;
  }
  
  .delete-button, .go-button {
    padding: 8px 16px;
    font-size: 13px;
  }
}
</style>
