<!-- 主应用模块 -->
<script setup>
import { ref, onMounted, watch } from 'vue';
import AddressBar from './components/AddressBar.vue';
import TimeDisplay from './components/TimeDisplay.vue';
import SearchBox from './components/SearchBox.vue';
import AppLauncher from './components/AppLauncher.vue';
import ContextMenu from './components/ContextMenu.vue';
import MoreOptions from './components/MoreOptions.vue';

const isSearchExpanded = ref(false);
const showMoreOptions = ref(false);

// 从本地存储读取应用列表
const loadApps = () => {
  try {
    const savedApps = localStorage.getItem('apps');
    return savedApps ? JSON.parse(savedApps) : [
      { id: 1, name: '标题1', icon: '🔧', description: '描述1', url: 'https://example.com' },
      { id: 2, name: '标题2', icon: '🎵', description: '描述2', url: 'https://example.com' },
      { id: 3, name: '标题3', icon: '🌐', description: '描述3', url: 'https://example.com' },
      { id: 4, name: '标题4', icon: '📱', description: '描述4', url: 'https://example.com' },
      { id: 5, name: '标题5', icon: '📁', description: '描述5', url: 'https://example.com' },
      { id: 6, name: '标题6', icon: '🐻', description: '描述6', url: 'https://example.com' },
      { id: 7, name: '标题7', icon: '🐱', description: '描述7', url: 'https://example.com' },
      { id: 8, name: '标题8', icon: '🐉', description: '描述8', url: 'https://example.com' },
      { id: 9, name: '标题9', icon: '☁️', description: '描述9', url: 'https://example.com' },
      { id: 10, name: '标题10', icon: '📖', description: '描述10', url: 'https://example.com' }
    ];
  } catch (error) {
    console.error('Failed to load apps from localStorage:', error);
    return [];
  }
};

// 应用列表
const apps = ref(loadApps());

// 监听应用列表变化，保存到本地存储
watch(apps, (newApps) => {
  try {
    localStorage.setItem('apps', JSON.stringify(newApps));
  } catch (error) {
    console.error('Failed to save apps to localStorage:', error);
  }
}, { deep: true });

// 处理右键菜单显示
const showContextMenu = (event, app) => {
  contextMenuRef.value.showContextMenu(event, app);
};

// 处理添加新应用
const handleAdd = () => {
  // 创建一个新的空应用对象
  const newApp = {
    id: Date.now(), // 使用时间戳作为唯一ID
    name: '',
    icon: '📄',
    description: '',
    url: '',
    openMethod: '_blank'
  };
  // 打开编辑对话框
  contextMenuRef.value.openEditDialog(newApp);
};

// 处理保存编辑
const handleSave = (app) => {
  // 检查是否是新应用（ID是否存在）
  const index = apps.value.findIndex(item => item.id === app.id);
  if (index !== -1) {
    // 更新现有应用
    apps.value[index] = app;
  } else {
    // 添加新应用
    apps.value.push(app);
  }
};

// 处理删除应用
const handleDelete = (app) => {
  const index = apps.value.findIndex(item => item.id === app.id);
  if (index !== -1) {
    apps.value.splice(index, 1);
  }
};

// 引用ContextMenu组件
const contextMenuRef = ref(null);

const handleSearchExpanded = (value) => {
  isSearchExpanded.value = value;
};

// 处理显示更多选项窗口
const handleShowMoreOptions = () => {
  showMoreOptions.value = true;
};

// 处理关闭更多选项窗口
const handleCloseMoreOptions = () => {
  showMoreOptions.value = false;
};
</script>

<template>
  <div class="wallpaper">
    <!-- 毛玻璃遮罩 -->
    <div v-if="isSearchExpanded" class="glass-mask" @click="isSearchExpanded = false"></div>
    
    <AddressBar @show-more-options="handleShowMoreOptions" />
    
    <div class="main-content">
      <div class="time-container">
        <TimeDisplay :blurred="isSearchExpanded" />
      </div>
      <div class="search-container-wrapper">
        <SearchBox :expanded="isSearchExpanded" @update:expanded="handleSearchExpanded" />
      </div>
      <AppLauncher :apps="apps" @contextmenu="showContextMenu" @add="handleAdd" />
    </div>
    
    <!-- 右键菜单和编辑对话框组件 -->
    <ContextMenu 
      ref="contextMenuRef"
      @save="handleSave"
      @delete="handleDelete"
    />
    
    <!-- 更多选项窗口组件 -->
    <MoreOptions v-if="showMoreOptions" @close="handleCloseMoreOptions" />
  </div>
</template>

<style scoped>
.wallpaper {
  width: 100vw;
  height: 100vh;
  background-image: url('/background.jpg');
  background-size: cover;
  background-position: center;
  background-repeat: no-repeat;
  display: flex;
  flex-direction: column;
  position: relative;
}

.main-content {
  flex: 1;
  position: relative;
}

.time-container {
  position: absolute;
  top: 20%;
  left: 50%;
  transform: translateX(-50%);
  z-index: 9000;
}

.search-container-wrapper {
  position: absolute;
  top: 35%;
  left: 50%;
  transform: translateX(-50%);
  z-index: 9000;
}

/* 毛玻璃遮罩 */
.glass-mask {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: rgba(0, 0, 0, 0.3);
  backdrop-filter: blur(5px);
  -webkit-backdrop-filter: blur(5px);
  z-index: 8999;
  pointer-events: auto;
}
</style>
