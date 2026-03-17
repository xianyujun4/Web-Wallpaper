<!-- 收藏夹组件 -->
<script setup>
import { ref, computed, onMounted } from 'vue';
import { useGlobalState } from '../composables/useGlobalState';

// 全局状态
const { currentEngine } = useGlobalState();

// 声明组件可以发射的事件
const emit = defineEmits(['close', 'go-to-home']);

// 收藏夹数据
const bookmarks = ref([]);
// 搜索关键词
const searchKeyword = ref('');
// 选中的收藏
const selectedBookmarks = ref([]);
// 新收藏表单
const newBookmark = ref({ title: '', url: '' });
// 显示添加收藏表单
const showAddForm = ref(false);

// 加载收藏夹
const loadBookmarks = () => {
  try {
    const savedBookmarks = localStorage.getItem('bookmarks');
    if (savedBookmarks) {
      let loadedBookmarks = JSON.parse(savedBookmarks);
      // 检查并修复重复的id
      const idSet = new Set();
      loadedBookmarks = loadedBookmarks.map(bookmark => {
        if (!bookmark.id || idSet.has(bookmark.id)) {
          const newId = Date.now() - Math.floor(Math.random() * 10000);
          idSet.add(newId);
          return { ...bookmark, id: newId };
        }
        idSet.add(bookmark.id);
        return bookmark;
      });
      bookmarks.value = loadedBookmarks;
      // 保存修复后的收藏夹
      localStorage.setItem('bookmarks', JSON.stringify(bookmarks.value));
    } else {
      // 模拟收藏夹数据
      bookmarks.value = [
        {
          id: Date.now() - 10000,
          title: 'Google',
          url: 'https://www.google.com',
          type: 'url',
          date: '2026-03-15'
        },
        {
          id: Date.now() - 9000,
          title: '百度',
          url: 'https://www.baidu.com',
          type: 'url',
          date: '2026-03-15'
        },
        {
          id: Date.now() - 8000,
          title: '命令 - 查看IP',
          url: '$ipconfig',
          type: 'command',
          date: '2026-03-14'
        },
        {
          id: Date.now() - 7000,
          title: '打开测试文件',
          url: '#open D:\\test.txt',
          type: 'command',
          date: '2026-03-14'
        }
      ];
      localStorage.setItem('bookmarks', JSON.stringify(bookmarks.value));
    }
  } catch (error) {
    console.error('Failed to load bookmarks:', error);
    bookmarks.value = [];
  }
};

// 过滤后的收藏夹
const filteredBookmarks = computed(() => {
  if (!searchKeyword.value) {
    return bookmarks.value;
  }
  const keyword = searchKeyword.value.toLowerCase();
  return bookmarks.value.filter(bookmark => 
    bookmark.title.toLowerCase().includes(keyword) || 
    bookmark.url.toLowerCase().includes(keyword)
  );
});

// 处理收藏点击
const handleBookmarkClick = (event, bookmark) => {
  event.stopPropagation();
  const index = selectedBookmarks.value.indexOf(bookmark.id);
  if (index === -1) {
    selectedBookmarks.value.push(bookmark.id);
  } else {
    selectedBookmarks.value.splice(index, 1);
  }
};

// 处理删除选中的收藏
const handleDeleteSelected = () => {
  bookmarks.value = bookmarks.value.filter(bookmark => 
    !selectedBookmarks.value.includes(bookmark.id)
  );
  selectedBookmarks.value = [];
  // 保存到本地存储
  localStorage.setItem('bookmarks', JSON.stringify(bookmarks.value));
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

// 辅助函数：使用隐藏 iframe 触发本地协议
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

// 处理前往选中的收藏
const handleGoToSelected = async () => {
  const selected = bookmarks.value.filter(bookmark => 
    selectedBookmarks.value.includes(bookmark.id)
  );

  if (selected.length === 0) return;

  const urls = [];
  const shellCommands = [];
  const customCommands = [];
  let isPopupBlocked = false;

  selected.forEach(bookmark => {
    const result = parseInput(bookmark.url);
    if (result.type === 'url' || result.type === 'search') {
      urls.push(bookmark.url);
    } else if (result.type === 'shell') {
      shellCommands.push(result.value);
    } else if (result.type === 'custom') {
      customCommands.push(result.value);
    }
  });

  // 处理网页和搜索
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

  // 处理所有命令
  const allCommands = [];
  
  // 添加 $ 指令
  if (shellCommands.length > 0) {
    const cleanShellCommands = shellCommands.map(cmd => cmd.replace(/\/$/, ''));
    allCommands.push(...cleanShellCommands);
  }
  
  // 添加 # 指令
  if (customCommands.length > 0) {
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
    const combinedCommand = allCommands.join(' & ');
    const iframe = document.createElement('iframe');
    iframe.style.display = 'none';
    iframe.src = 'mycmd://' + encodeURIComponent(combinedCommand);
    document.body.appendChild(iframe);
    // 立即移除 iframe
    setTimeout(() => {
      document.body.removeChild(iframe);
    }, 100);
  }

  if (isPopupBlocked) {
    alert('部分网页被浏览器拦截了！\n请点击浏览器地址栏右侧的🚫图标，选择"始终允许弹出式窗口"。');
  }

  emit('go-to-home');
};

// 处理添加新收藏
const handleAddBookmark = () => {
  if (!newBookmark.value.title || !newBookmark.value.url) {
    alert('请填写标题和URL');
    return;
  }

  const newItem = {
    id: Date.now(),
    title: newBookmark.value.title,
    url: newBookmark.value.url,
    type: parseInput(newBookmark.value.url).type,
    date: new Date().toISOString().split('T')[0]
  };

  bookmarks.value.push(newItem);
  localStorage.setItem('bookmarks', JSON.stringify(bookmarks.value));
  
  // 重置表单
  newBookmark.value = { title: '', url: '' };
  showAddForm.value = false;
};

// 处理关闭窗口
const handleClose = () => {
  emit('close');
};

// 组件挂载时加载收藏夹
onMounted(() => {
  loadBookmarks();
});
</script>

<template>
  <div class="bookmarks-overlay" @click.stop>
    <div class="bookmarks-window">
      <div class="bookmarks-header">
        <h3>收藏夹</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>
      
      <!-- 收藏夹搜索框和添加按钮 -->
      <div class="bookmarks-search">
        <div class="search-container">
          <input 
            type="text" 
            v-model="searchKeyword" 
            placeholder="搜索收藏" 
            class="search-input"
            @click.stop
          />
          <button class="add-button" @click.stop="showAddForm = !showAddForm">
            + 添加
          </button>
        </div>
        
        <!-- 添加收藏表单 -->
        <div v-if="showAddForm" class="add-form">
          <input 
            type="text" 
            v-model="newBookmark.title" 
            placeholder="标题" 
            class="form-input"
            @click.stop
          />
          <input 
            type="text" 
            v-model="newBookmark.url" 
            placeholder="URL 或命令" 
            class="form-input"
            @click.stop
          />
          <div class="form-buttons">
            <button class="cancel-button" @click.stop="showAddForm = false">取消</button>
            <button class="save-button" @click.stop="handleAddBookmark">保存</button>
          </div>
        </div>
      </div>
      
      <!-- 收藏夹内容 -->
      <div class="bookmarks-content">
        <div v-if="filteredBookmarks.length === 0" class="empty-bookmarks">
          没有收藏
        </div>
        <div v-else>
          <div 
            v-for="bookmark in filteredBookmarks" 
            :key="bookmark.id"
            class="bookmark-item"
            :class="{ 'selected': selectedBookmarks.includes(bookmark.id) }"
            @click="handleBookmarkClick($event, bookmark)"
          >
            <div class="bookmark-title">{{ bookmark.title }}</div>
            <div class="bookmark-url">{{ bookmark.url }}</div>
            <div class="bookmark-type" :class="bookmark.type">
              {{ bookmark.type === 'url' ? '网页' : '命令' }}
            </div>
          </div>
        </div>
      </div>
      
      <!-- 底部按钮 -->
      <div class="bookmarks-footer">
        <button class="delete-button" @click.stop="handleDeleteSelected">删除</button>
        <button class="go-button" @click.stop="handleGoToSelected">前往</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 收藏夹窗口覆盖层 */
.bookmarks-overlay {
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

/* 收藏夹窗口 */
.bookmarks-window {
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

/* 收藏夹窗口头部 */
.bookmarks-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.bookmarks-header h3 {
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

/* 收藏夹搜索框 */
.bookmarks-search {
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
  margin-bottom: 10px;
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

.search-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

.add-button {
  padding: 10px 15px;
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
  border: none;
  border-radius: 20px;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
  white-space: nowrap;
}

.add-button:hover {
  background-color: rgba(76, 175, 80, 0.3);
  color: rgba(76, 175, 80, 1);
}

/* 添加收藏表单 */
.add-form {
  margin-top: 10px;
  padding: 15px;
  background-color: rgba(255, 255, 255, 0.05);
  border-radius: 8px;
}

.form-input {
  width: 100%;
  padding: 10px 15px;
  background-color: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 14px;
  outline: none;
  box-sizing: border-box;
  margin-bottom: 10px;
}

.form-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

.form-buttons {
  display: flex;
  gap: 10px;
  justify-content: flex-end;
}

.cancel-button, .save-button {
  padding: 8px 16px;
  border: none;
  border-radius: 6px;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.cancel-button {
  background-color: rgba(255, 255, 255, 0.1);
  color: rgba(255, 255, 255, 0.7);
}

.cancel-button:hover {
  background-color: rgba(255, 255, 255, 0.15);
  color: rgba(255, 255, 255, 0.9);
}

.save-button {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

.save-button:hover {
  background-color: rgba(76, 175, 80, 0.3);
  color: rgba(76, 175, 80, 1);
}

/* 收藏夹内容 */
.bookmarks-content {
  flex: 1;
  overflow-y: auto;
  margin-bottom: 15px;
}

/* 空收藏夹 */
.empty-bookmarks {
  text-align: center;
  padding: 40px 0;
  color: rgba(255, 255, 255, 0.5);
  font-size: 14px;
}

/* 收藏项 */
.bookmark-item {
  padding: 15px;
  cursor: pointer;
  transition: all 0.2s ease;
  position: relative;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.bookmark-item:hover {
  background-color: rgba(255, 255, 255, 0.05);
}

.bookmark-item.selected {
  background-color: rgba(255, 255, 255, 0.1);
  border-left: 3px solid rgba(255, 255, 255, 0.5);
}

/* 收藏标题 */
.bookmark-title {
  font-size: 14px;
  font-weight: 500;
  margin-bottom: 5px;
  color: rgba(255, 255, 255, 0.9);
}

/* 收藏URL */
.bookmark-url {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
  margin-bottom: 8px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* 收藏类型 */
.bookmark-type {
  font-size: 11px;
  padding: 2px 8px;
  border-radius: 10px;
  display: inline-block;
}

.bookmark-type.url {
  background-color: rgba(33, 150, 243, 0.2);
  color: rgba(33, 150, 243, 0.9);
}

.bookmark-type.command {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

/* 收藏夹窗口底部 */
.bookmarks-footer {
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
  display: flex;
  align-items: center;
  justify-content: center;
}

.delete-button {
  background-color: rgba(255, 87, 34, 0.2);
  color: rgba(255, 87, 34, 0.9);
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

/* 滚动条样式 */
.bookmarks-content::-webkit-scrollbar {
  width: 8px;
}

.bookmarks-content::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
}

.bookmarks-content::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.2);
  border-radius: 4px;
}

.bookmarks-content::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.3);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .bookmarks-window {
    width: 95%;
    height: 85%;
    padding: 15px;
  }
  
  .search-container {
    flex-direction: column;
  }
  
  .add-button {
    align-self: flex-start;
  }
  
  .bookmark-title {
    font-size: 13px;
  }
  
  .bookmark-url {
    font-size: 11px;
  }
  
  .bookmark-type {
    font-size: 10px;
    padding: 2px 6px;
  }
  
  .delete-button, .go-button {
    padding: 8px 16px;
    font-size: 13px;
  }
}
</style>