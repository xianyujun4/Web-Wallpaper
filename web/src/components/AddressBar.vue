<!-- 地址栏模块 -->
<script setup>
import { ref, watch } from 'vue';
import { addressInput, executeInput } from '../composables/useAddressBar';

// 声明组件可以发射的事件
const emit = defineEmits(['show-more-options']);

const isVisible = ref(false);

const handleMouseEnter = () => {
  isVisible.value = true;
};

const handleMouseLeave = () => {
  if (!addressInput.value) {
    isVisible.value = false;
  }
};

// 监听输入变化，当有输入时保持显示
watch(addressInput, (newValue) => {
  if (newValue) {
    isVisible.value = true;
  }
});

// 处理更多按钮点击
const handleMoreClick = () => {
  // 向父组件发送显示更多选项窗口的事件
  emit('show-more-options');
};
</script>

<template>
  <div class="address-bar-wrapper" @mouseenter="handleMouseEnter" @mouseleave="handleMouseLeave">
    <div class="address-bar" :class="{ 'visible': isVisible }">
      <div class="address-input-container">
        <input 
          type="text" 
          class="address-input" 
          placeholder="输入地址或搜索" 
          v-model="addressInput"
          @keydown.enter="executeInput(addressInput)"
        />
        <!-- 执行按钮 -->
        <div class="execute-button" @click="executeInput(addressInput)">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="11" cy="11" r="8"></circle>
            <path d="m21 21-4.35-4.35"></path>
          </svg>
        </div>
      </div>
      <div class="menu-button" @click="handleMoreClick">
        <div class="dot"></div>
        <div class="dot"></div>
        <div class="dot"></div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.address-bar-wrapper {
  width: 100%;
  height: 60px;
  position: fixed;
  top: 0;
  left: 0;
  z-index: 1004;
}

.address-bar {
  width: 100%;
  height: 60px;
  background-color: transparent;
  display: flex;
  align-items: center;
  padding: 0 20px;
  box-sizing: border-box;
  transition: opacity 0.3s ease, transform 0.3s ease;
  opacity: 0;
  transform: translateY(-20px);
}

.address-bar.visible {
  opacity: 1;
  transform: translateY(0);
}

.address-input-container {
  flex: 1;
  display: flex;
  align-items: center;
  background-color: rgba(0, 0, 0, 0.3);
  backdrop-filter: blur(5px);
  -webkit-backdrop-filter: blur(5px);
  border-radius: 20px;
  overflow: hidden;
}

.address-input {
  flex: 1;
  height: 40px;
  border: none;
  padding: 0 20px;
  font-size: 16px;
  outline: none;
  text-align: center;
  background-color: transparent;
  color: rgba(255, 255, 255, 0.9);
}

/* 执行按钮 */
.execute-button {
  width: 40px;
  height: 40px;
  display: flex;
  justify-content: center;
  align-items: center;
  color: rgba(255, 255, 255, 0.7);
  cursor: pointer;
  transition: color 0.3s ease;
  flex-shrink: 0;
}

.execute-button:hover {
  color: rgba(255, 255, 255, 0.9);
}

.address-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

.menu-button {
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  width: 40px;
  height: 40px;
  margin-left: 10px;
  cursor: pointer;
}

.dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: rgba(0, 0, 0, 0.5);
  margin: 2px 0;
}
</style>
