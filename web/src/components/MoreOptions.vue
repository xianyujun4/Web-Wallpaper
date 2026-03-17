<!-- 更多选项窗口组件 -->
<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
import HistoryRecord from './HistoryRecord.vue';
import Bookmarks from './Bookmarks.vue';
import HomeEdit from './HomeEdit.vue';
import CustomCommands from './CustomCommands.vue';
import Plugins from './Plugins.vue';

// 声明组件可以发射的事件
const emit = defineEmits(['close', 'update:apps', 'update:wallpaper']);

// 声明组件可以接收的属性
const props = defineProps({
  apps: {
    type: Array,
    required: true
  }
});

// 选项列表
const options = [
  { id: 'home', name: '主页编辑' },
  { id: 'history', name: '历史记录' },
  { id: 'bookmarks', name: '收藏夹' },
  { id: 'commands', name: '自定义指令' },
  { id: 'plugins', name: '插件' }
];

// 控制历史记录窗口的显示
const showHistory = ref(false);
// 控制收藏夹窗口的显示
const showBookmarks = ref(false);
// 控制主页编辑窗口的显示
const showHomeEdit = ref(false);
// 控制自定义指令窗口的显示
const showCustomCommands = ref(false);
// 控制插件窗口的显示
const showPlugins = ref(false);

// 处理选项点击
const handleOptionClick = (option) => {
  console.log('Clicked option:', option);
  if (option.id === 'history') {
    showHistory.value = true;
  } else if (option.id === 'bookmarks') {
    showBookmarks.value = true;
  } else if (option.id === 'home') {
    showHomeEdit.value = true;
  } else if (option.id === 'commands') {
    showCustomCommands.value = true;
  } else if (option.id === 'plugins') {
    showPlugins.value = true;
  }
  // 这里可以添加其他选项的处理逻辑
};

// 处理关闭历史记录窗口
const handleCloseHistory = () => {
  showHistory.value = false;
};

// 处理历史记录跳转后回到主页
const handleGoToHome = () => {
  showHistory.value = false;
  showBookmarks.value = false;
  showHomeEdit.value = false;
  showCustomCommands.value = false;
  showPlugins.value = false;
  handleClose();
};

// 处理关闭收藏夹窗口
const handleCloseBookmarks = () => {
  showBookmarks.value = false;
};

// 处理关闭主页编辑窗口
const handleCloseHomeEdit = () => {
  showHomeEdit.value = false;
};

// 处理关闭自定义指令窗口
const handleCloseCustomCommands = () => {
  showCustomCommands.value = false;
};

// 处理关闭插件窗口
const handleClosePlugins = () => {
  showPlugins.value = false;
};

// 处理更新应用列表
const handleUpdateApps = (updatedApps) => {
  emit('update:apps', updatedApps);
};

// 处理更新壁纸
const handleUpdateWallpaper = (wallpaper) => {
  emit('update:wallpaper', wallpaper);
};

// 处理关闭窗口
const handleClose = () => {
  emit('close');
};

// 处理点击空白处关闭窗口
const handleClickOutside = (event) => {
  // 暂时注释掉这个功能，避免optionsRef引用问题
  // if (optionsRef.value && !optionsRef.value.contains(event.target)) {
  //   handleClose();
  // }
};

// 选项窗口引用 - 暂时不需要
// const optionsRef = ref(null);

// 组件挂载时添加事件监听器 - 暂时不需要
// onMounted(() => {
//   document.addEventListener('click', handleClickOutside);
// });

// 组件卸载时移除事件监听器 - 暂时不需要
// onUnmounted(() => {
//   document.removeEventListener('click', handleClickOutside);
// });
</script>

<template>
  <div class="more-options-overlay">
    <div class="more-options" @click.stop>
      <div class="more-options-header">
        <h3>更多选项</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>
      <div class="more-options-content">
        <div 
          class="option-item" 
          v-for="option in options" 
          :key="option.id"
          @click="handleOptionClick(option)"
        >
          {{ option.name }}
        </div>
      </div>
    </div>
    <HistoryRecord v-if="showHistory" @close="handleCloseHistory" @go-to-home="handleGoToHome" />
    <Bookmarks v-if="showBookmarks" @close="handleCloseBookmarks" @go-to-home="handleGoToHome" />
    <HomeEdit 
      v-if="showHomeEdit" 
      :apps="props.apps"
      @close="handleCloseHomeEdit" 
      @go-to-home="handleGoToHome"
      @update:apps="handleUpdateApps"
      @update:wallpaper="handleUpdateWallpaper"
    />
    <CustomCommands v-if="showCustomCommands" @close="handleCloseCustomCommands" />
    <Plugins v-if="showPlugins" @close="handleClosePlugins" />
  </div>
</template>

<style scoped>
/* 更多选项窗口覆盖层 */
.more-options-overlay {
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
  z-index: 9998;
  opacity: 1;
  transition: opacity 0.3s ease;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  pointer-events: auto;
}

.more-options-overlay.show {
  opacity: 1;
}

/* 更多选项窗口 */
.more-options {
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  padding: 20px;
  width: 90%;
  max-width: 300px;
  color: rgba(255, 255, 255, 0.9);
  transform: scale(1);
  transition: transform 0.3s ease;
  margin: auto;
  position: relative;
  z-index: 9999;
  pointer-events: auto;
}

/* 更多选项窗口头部 */
.more-options-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.more-options-header h3 {
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

/* 选项内容 */
.more-options-content {
  display: flex;
  flex-direction: column;
  gap: 0;
}

.option-item {
  padding: 15px 20px;
  cursor: pointer;
  transition: all 0.2s ease;
  font-size: 14px;
  text-align: center;
  position: relative;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.option-item:last-child {
  border-bottom: none;
}

.option-item:hover {
  background-color: rgba(255, 255, 255, 0.05);
  transform: translateY(-1px);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .more-options {
    width: 95%;
    padding: 15px;
  }
  
  .option-item {
    padding: 10px 14px;
    font-size: 13px;
  }
}
</style>
