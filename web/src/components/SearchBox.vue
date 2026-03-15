<!-- 搜索框模块 -->
<script setup>
import { useSearch } from '../composables/useSearch';
import { defineProps, watch } from 'vue';

const props = defineProps({
  expanded: {
    type: Boolean,
    default: false
  }
});

const {
  searchQuery,
  displayHistory,
  toggleEngine,
  performSearch,
  handleKeyPress,
  selectHistory,
  deleteHistory,
  getCurrentEngineName,
  closeSearch
} = useSearch();

const emit = defineEmits(['update:expanded']);

const handleExpand = () => {
  emit('update:expanded', true);
};

// 清除显示的搜索记录
const clearDisplayHistory = () => {
  // 只清除当前显示的五条记录
  for (let i = 0; i < 5 && displayHistory.value.length > 0; i++) {
    const item = displayHistory.value[0];
    deleteHistory(item);
  }
};

// 监听展开状态变化，当从展开变为收起时，清空搜索框
watch(() => props.expanded, (newVal, oldVal) => {
  if (oldVal && !newVal) {
    // 从展开变为收起，清空搜索框
    searchQuery.value = '';
  }
});
</script>

<template>
  <div class="search-wrapper">
    <div class="search-container" :class="{ 'expanded': props.expanded }" @click="handleExpand">
      <div class="search-input-wrapper" @click.stop="handleExpand">
        <!-- 搜索引擎切换按钮 -->
        <div class="engine-toggle" @click.stop="toggleEngine">
          {{ getCurrentEngineName() }}
        </div>
        
        <!-- 搜索输入框 -->
        <input 
          type="text" 
          class="search-input" 
          v-model="searchQuery"
          placeholder="搜索"
          @keypress="handleKeyPress"
          @click.stop="handleExpand"
        />
        
        <!-- 放大镜图标 -->
        <div class="search-button" @click.stop="performSearch">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="11" cy="11" r="8"></circle>
            <path d="m21 21-4.35-4.35"></path>
          </svg>
        </div>
      </div>
      
      <!-- 搜索历史 -->
        <transition name="slide">
          <div v-if="props.expanded" class="search-history" @click.stop>
            <div class="history-item" v-for="(item, index) in displayHistory" :key="index">
              <span class="history-text" @click="selectHistory(item)">{{ item }}</span>
              <span class="history-delete" @click.stop="deleteHistory(item)">×</span>
            </div>
            <div class="history-clear" @click="clearDisplayHistory">
              清除记录
            </div>
          </div>
        </transition>
    </div>
  </div>
</template>

<style scoped>
.search-wrapper {
  display: flex;
  justify-content: center;
  width: 100%;
}

.search-container {
  width: 400px;
  max-width: 80%;
  transition: width 0.3s ease;
  position: relative;
  z-index: 9000;
}

.search-container:hover {
  width: 600px;
  max-width: 120%;
}

.search-container.expanded {
  width: 600px !important;
  max-width: 120% !important;
}

.search-input-wrapper {
  display: flex;
  align-items: center;
  background-color: transparent;
  border-radius: 20px;
  overflow: visible;
}

/* 搜索引擎切换按钮 */
.engine-toggle {
  width: 40px;
  height: 40px;
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: rgba(0, 0, 0, 0.8);
  cursor: pointer;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.7);
  border-radius: 20px 0 0 20px;
  flex-shrink: 0;
}

/* 搜索输入框 */
.search-input {
  flex: 1;
  height: 40px;
  border: none;
  padding: 0 20px;
  font-size: 16px;
  outline: none;
  text-align: center;
  background-color: rgba(0, 0, 0, 0.8);
  color: rgba(255, 255, 255, 0.9);
}

.search-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

/* 搜索按钮 */
.search-button {
  width: 40px;
  height: 40px;
  display: flex;
  justify-content: center;
  align-items: center;
  color: rgba(255, 255, 255, 0.7);
  cursor: pointer;
  transition: color 0.3s ease;
  background-color: rgba(0, 0, 0, 0.8);
  border-radius: 0 20px 20px 0;
  flex-shrink: 0;
}

.search-button:hover {
  color: rgba(255, 255, 255, 0.9);
}

/* 搜索历史 */
.search-history {
  position: absolute;
  top: 100%;
  left: 40px;
  right: 40px;
  background-color: rgba(0, 0, 0, 0.3) !important;
  border-radius: 0 0 20px 20px;
  padding: 10px 0;
  box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
  z-index: 9001;
  backdrop-filter: blur(5px) !important;
  -webkit-backdrop-filter: blur(5px) !important;
}

.history-item {
  padding: 10px 20px;
  cursor: pointer;
  transition: background-color 0.2s ease;
  color: rgba(255, 255, 255, 0.9) !important;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.history-item:hover {
  background-color: rgba(255, 255, 255, 0.1);
}

.history-text {
  flex: 1;
  cursor: pointer;
}

.history-delete {
  margin-left: 10px;
  cursor: pointer;
  font-size: 18px;
  color: rgba(255, 255, 255, 0.7);
  transition: color 0.2s ease;
}

.history-delete:hover {
  color: rgba(255, 255, 255, 0.9);
}

.history-clear {
  padding: 10px 20px;
  text-align: center;
  cursor: pointer;
  transition: background-color 0.2s ease;
  color: rgba(255, 255, 255, 0.7) !important;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  margin-top: 5px;
}

.history-clear:hover {
  background-color: rgba(255, 255, 255, 0.1);
  color: rgba(255, 255, 255, 0.9) !important;
}

/* 搜索历史过渡动画 */
.slide-enter-active,
.slide-leave-active {
  transition: all 0.3s ease;
}

.slide-enter-from {
  opacity: 0;
  transform: translateY(-10px);
}

.slide-enter-to {
  opacity: 1;
  transform: translateY(0);
}

.slide-leave-from {
  opacity: 1;
  transform: translateY(0);
}

.slide-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}
</style>
