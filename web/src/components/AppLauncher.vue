<!-- 应用启动器模块 -->
<script setup>
import { computed } from 'vue';
import { executeInput } from '../composables/useAddressBar';

// 声明组件可以接收的属性
const props = defineProps({
  apps: {
    type: Array,
    required: true
  }
});

// 声明组件可以发射的事件
const emit = defineEmits(['contextmenu', 'add']);

// 按行划分应用，每行5个
const lines = computed(() => {
  const itemsPerLine = 5;
  const result = [];
  
  // 按行划分应用
  for (let i = 0; i < props.apps.length; i += itemsPerLine) {
    result.push(props.apps.slice(i, i + itemsPerLine));
  }
  
  return result;
});

// 处理按钮点击事件
const handleButtonClick = async (app) => {
  if (app.url) {
    // 检查是否是自定义指令（以@开头）
    if (app.url.startsWith('@')) {
      const commandName = app.url.substring(1);
      // 执行自定义指令
      await executeInput(`#${commandName}`);
    } else {
      // 找到地址栏元素
      const addressBar = document.querySelector('.address-bar');
      const addressInputEl = document.querySelector('.address-input');
      
      if (addressBar && addressInputEl) {
        // 显示地址栏
        addressBar.classList.add('visible');
        // 输入URL
        addressInputEl.value = app.url;
        
        // 使用地址栏的执行函数来执行输入内容
        await executeInput(app.url);
      }
    }
  }
};

// 打开应用
const openApp = (app) => {
  const url = app.url;
  const openMethod = app.openMethod || '_blank';
  
  // 检查是否是命令（以#开头）
  if (url.startsWith('#')) {
    // 执行命令
    const command = url.substring(1);
    window.location.href = `mycmd://${command}`;
  } else if (url.startsWith('http://') || url.startsWith('https://')) {
    // 打开URL
    window.open(url, openMethod);
  } else {
    // 尝试作为本地路径或其他命令打开
    window.location.href = `mycmd://start "${url}"`;
  }
};

// 添加新应用
const addApp = () => {
  // 向父组件发送添加新应用的事件
  emit('add');
};

</script>

<template>
  <div class="app-launcher">
    <div class="app-list">
      <!-- 应用列表容器 -->
      <div class="app-container">
        <!-- 应用按钮 -->
        <div 
          class="app-item" 
          v-for="app in props.apps" 
          :key="app.id"
          @click="handleButtonClick(app)"
          @contextmenu="$emit('contextmenu', $event, app)"
        >
          <div class="app-icon">
            <img v-if="app.icon && (app.icon.startsWith('http://') || app.icon.startsWith('https://'))" :src="app.icon" alt="Icon" style="width: 24px; height: 24px;" />
            <span v-else>{{ app.icon }}</span>
          </div>
          <div class="app-info">
            <div class="app-name">{{ app.name }}</div>
            <div class="app-description">{{ app.description }}</div>
          </div>
        </div>
        
        <!-- 常驻的按钮添加按钮 -->
        <div 
          class="app-item add-button"
          @click="addApp"
          @contextmenu.stop
        >
          <div class="app-icon add-icon">+</div>
          <div class="app-info">
            <div class="app-name"></div>
            <div class="app-description"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.app-launcher {
  position: absolute;
  top: 42%;
  left: 50%;
  transform: translateX(-50%);
  z-index: 1000;
  overflow: visible;
}

.app-list {
  display: flex;
  flex-direction: column;
  gap: 15px;
  justify-content: center;
  position: relative;
  z-index: 1000;
  overflow: visible;
}

.app-container {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 15px;
  justify-content: center;
  align-items: center;
  position: relative;
  z-index: 1000;
  overflow: visible;
}

/* 响应式设计 */
@media (max-width: 1200px) {
  .app-container {
    grid-template-columns: repeat(4, 1fr);
  }
}

@media (max-width: 992px) {
  .app-container {
    grid-template-columns: repeat(3, 1fr);
  }
}

@media (max-width: 768px) {
  .app-container {
    grid-template-columns: repeat(2, 1fr);
  }
}

.app-item {
  display: flex;
  align-items: center;
  padding: 12px 15px;
  background-color: rgba(0, 0, 0, 0.3);
  backdrop-filter: blur(5px);
  -webkit-backdrop-filter: blur(5px);
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.3s ease;
  min-width: 200px;
  max-width: 250px;
  position: relative;
  z-index: 1000;
  overflow: visible;
}

.app-item:hover {
  background-color: rgba(0, 0, 0, 0.5);
  transform: translateY(-3px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
}

/* 添加按钮样式 */
.add-button {
  cursor: pointer;
}

.add-icon {
  font-size: 28px;
  font-weight: bold;
  color: rgba(255, 255, 255, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
}

.app-icon {
  font-size: 24px;
  margin-right: 12px;
  width: 40px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: rgba(255, 255, 255, 0.1);
  border-radius: 6px;
}

.app-info {
  flex: 1;
  min-width: 0;
}

.app-name {
  font-size: 14px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
  margin-bottom: 4px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.app-description {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.7);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}



/* 编辑对话框 */
.edit-dialog-overlay {
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
  z-index: 2000;
  opacity: 0;
  transition: opacity 0.3s ease;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
}

.edit-dialog-overlay.show {
  opacity: 1;
}

.edit-dialog {
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  padding: 20px;
  width: 90%;
  max-width: 500px;
  color: rgba(255, 255, 255, 0.9);
  transform: scale(0.9);
  transition: transform 0.3s ease;
  margin: auto;
}

.edit-dialog-overlay.show .edit-dialog {
  transform: scale(1);
}

.edit-dialog-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}

.edit-dialog-header h3 {
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

.form-group {
  margin-bottom: 15px;
}

.form-group label {
  display: block;
  margin-bottom: 5px;
  font-size: 14px;
  color: rgba(255, 255, 255, 0.7);
}

.form-group input,
.form-group select {
  width: 100%;
  padding: 8px 12px;
  background-color: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 14px;
  box-sizing: border-box;
  transition: all 0.2s ease;
}

.form-group input:focus,
.form-group select:focus {
  outline: none;
  border-color: rgba(255, 255, 255, 0.4);
  background-color: rgba(255, 255, 255, 0.15);
}

.edit-dialog-footer {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  margin-top: 20px;
}

.delete-button,
.save-button {
  padding: 8px 16px;
  border: none;
  border-radius: 6px;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.delete-button {
  background-color: rgba(255, 87, 34, 0.2);
  color: rgba(255, 87, 34, 0.9);
}

.delete-button:hover {
  background-color: rgba(255, 87, 34, 0.3);
  color: rgba(255, 87, 34, 1);
}

.save-button {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

.save-button:hover {
  background-color: rgba(76, 175, 80, 0.3);
  color: rgba(76, 175, 80, 1);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .app-item {
    min-width: 180px;
    max-width: 220px;
    padding: 10px 12px;
  }
  
  .app-icon {
    font-size: 20px;
    width: 36px;
    height: 36px;
    margin-right: 10px;
  }
  
  .app-name {
    font-size: 13px;
  }
  
  .app-description {
    font-size: 11px;
  }
  
  .edit-dialog {
    width: 95%;
    padding: 15px;
  }
  
  .form-group input,
  .form-group select {
    padding: 6px 10px;
    font-size: 13px;
  }
}
</style>