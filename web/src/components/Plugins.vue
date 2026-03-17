<!-- 插件模块组件 -->
<script setup>
import { ref } from 'vue';

// 声明组件可以发射的事件
const emit = defineEmits(['close']);

// 处理关闭窗口
const handleClose = () => {
  emit('close');
};
</script>

<template>
  <div class="plugins-overlay" @click.stop>
    <div class="plugins-window">
      <div class="plugins-header">
        <h3>插件管理</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>
      
      <!-- 插件内容 -->
      <div class="plugins-content">
        <div class="developing-message">
          <div class="developing-icon">🔧</div>
          <h4>正在开发中</h4>
          <p>插件功能正在开发中，敬请期待！</p>
        </div>
      </div>
      
      <!-- 底部按钮 -->
      <div class="plugins-footer">
        <button class="close-button-large" @click.stop="handleClose">关闭</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 插件窗口覆盖层 */
.plugins-overlay {
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

/* 插件窗口 */
.plugins-window {
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  padding: 20px;
  width: 90%;
  max-width: 600px;
  height: 95%;
  max-height: 500px;
  color: rgba(255, 255, 255, 0.9);
  margin: auto;
  position: relative;
  z-index: 10000;
  pointer-events: auto;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* 插件窗口头部 */
.plugins-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.plugins-header h3 {
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

/* 插件内容 */
.plugins-content {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
}

.developing-message {
  text-align: center;
  padding: 40px 20px;
  background-color: rgba(255, 255, 255, 0.05);
  border-radius: 12px;
  width: 100%;
  max-width: 400px;
}

.developing-icon {
  font-size: 48px;
  margin-bottom: 20px;
}

.developing-message h4 {
  margin: 0 0 10px 0;
  font-size: 18px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
}

.developing-message p {
  margin: 0;
  font-size: 14px;
  color: rgba(255, 255, 255, 0.7);
  line-height: 1.5;
}

/* 插件窗口底部 */
.plugins-footer {
  display: flex;
  justify-content: center;
  gap: 10px;
  padding-top: 20px;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  margin-top: auto;
}

/* 底部按钮 */
.close-button-large {
  padding: 10px 30px;
  background-color: rgba(255, 255, 255, 0.1);
  border: none;
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.7);
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.close-button-large:hover {
  background-color: rgba(255, 255, 255, 0.15);
  color: rgba(255, 255, 255, 0.9);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .plugins-window {
    width: 95%;
    height: 85%;
    padding: 15px;
  }
  
  .developing-icon {
    font-size: 36px;
    margin-bottom: 15px;
  }
  
  .developing-message h4 {
    font-size: 16px;
  }
  
  .developing-message p {
    font-size: 13px;
  }
  
  .close-button-large {
    padding: 8px 20px;
    font-size: 13px;
  }
}
</style>