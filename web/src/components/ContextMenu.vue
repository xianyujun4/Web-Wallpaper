<!-- 右键菜单和编辑对话框模块 -->
<script setup>
import { ref, onMounted, onUnmounted } from 'vue';

// 声明组件可以发射的事件
const emit = defineEmits(['save', 'delete']);

// 右键菜单状态
const contextMenu = ref({
  visible: false,
  x: 0,
  y: 0,
  app: null
});

// 编辑对话框状态
const editDialog = ref({
  visible: false,
  app: null
});

// 菜单元素引用
const menuRef = ref(null);

// 显示右键菜单
const showContextMenu = (event, app) => {
  event.preventDefault();
  
  // 直接使用鼠标点击位置
  let x = event.clientX;
  let y = event.clientY;

  // 确保菜单不会超出窗口边界
  const menuWidth = 120;
  const menuHeight = 70;
  
  if (x + menuWidth > window.innerWidth) {
    x = window.innerWidth - menuWidth;
  }

  if (y + menuHeight > window.innerHeight) {
    y = window.innerHeight - menuHeight;
  }

  contextMenu.value = {
    visible: true,
    x,
    y,
    app
  };
};

// 隐藏右键菜单
const hideContextMenu = () => {
  contextMenu.value.visible = false;
};

// 处理点击空白处关闭菜单
const handleClickOutside = (event) => {
  if (contextMenu.value.visible && menuRef.value && !menuRef.value.contains(event.target)) {
    hideContextMenu();
  }
};

// 打开编辑对话框
const openEditDialog = (app) => {
  editDialog.value = {
    visible: true,
    app: { ...app }
  };
  hideContextMenu();
};

// 保存编辑
const saveEdit = async () => {
  const app = editDialog.value.app;
  
  // 只有当图标栏没有输入内容时才自动替换图标
  if (!app.icon || app.icon.trim() === '') {
    // 根据URL类型设置图标
    if (app.url) {
      // 如果是$开头的指令，图标设为$
      if (app.url.startsWith('$')) {
        app.icon = '$';
      }
      // 如果是#开头的指令，图标设为#
      else if (app.url.startsWith('#')) {
        app.icon = '#';
      }
      // 如果是网址，则尝试获取网站图标
      else if (app.url.startsWith('http://') || app.url.startsWith('https://')) {
        try {
          // 提取域名
          const url = new URL(app.url);
          const domain = url.origin;
          
          // 构建favicon URL
          const faviconUrl = `${domain}/favicon.ico`;
          
          // 直接使用favicon URL作为图标
          // 浏览器会自动处理不存在的情况
          app.icon = faviconUrl;
        } catch (error) {
          console.error('Failed to get favicon:', error);
        }
      }
    }
  }
  
  emit('save', app);
  editDialog.value.visible = false;
};

// 删除应用
const deleteApp = () => {
  emit('delete', contextMenu.value.app || editDialog.value.app);
  editDialog.value.visible = false;
  hideContextMenu();
};

// 组件挂载时添加事件监听器
onMounted(() => {
  document.addEventListener('click', handleClickOutside);
});

// 组件卸载时移除事件监听器
onUnmounted(() => {
  document.removeEventListener('click', handleClickOutside);
});

// 暴露方法给父组件
defineExpose({
  showContextMenu,
  hideContextMenu,
  openEditDialog
});
</script>

<template>
  <!-- 右键菜单 -->
  <div 
    v-if="contextMenu.visible" 
    class="context-menu"
    :style="{ left: contextMenu.x + 'px', top: contextMenu.y + 'px' }"
    ref="menuRef"
  >
    <div class="context-menu-item" @click="openEditDialog(contextMenu.app)">
      编辑
    </div>
    <div class="context-menu-item" @click="deleteApp">
      删除
    </div>
  </div>
  
  <!-- 编辑对话框 -->
  <div v-if="editDialog.visible" class="edit-dialog-overlay show">
    <div class="edit-dialog" @click.stop>
      <div class="edit-dialog-header">
        <h3>修改项目</h3>
        <button class="close-button" @click="editDialog.visible = false">×</button>
      </div>
      <div class="edit-dialog-content">
        <div class="form-group">
          <label>图标</label>
          <input type="text" v-model="editDialog.app.icon" placeholder="输入图标或图片地址">
        </div>
        <div class="form-group">
          <label>标题</label>
          <input type="text" v-model="editDialog.app.name" placeholder="输入标题" required>
        </div>
        <div class="form-group">
          <label>描述信息</label>
          <input type="text" v-model="editDialog.app.description" placeholder="输入描述信息">
        </div>
        <div class="form-group">
          <label>按钮指向</label>
          <input type="text" v-model="editDialog.app.url" placeholder="输入命令或URL" required>
        </div>
      </div>
      <div class="edit-dialog-footer">
        <button class="delete-button" @click="deleteApp">删除</button>
        <button class="save-button" @click="saveEdit">保存</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 右键菜单 */
.context-menu {
  position: fixed;
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 8px;
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.3);
  z-index: 9999;
  min-width: 120px;
  pointer-events: auto;
}

.context-menu-item {
  padding: 8px 12px;
  color: rgba(255, 255, 255, 0.9);
  cursor: pointer;
  transition: background-color 0.2s ease;
  border-radius: 4px;
  margin: 4px;
}

.context-menu-item:hover {
  background-color: rgba(255, 255, 255, 0.1);
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
  z-index: 9998;
  opacity: 0;
  transition: opacity 0.3s ease;
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  pointer-events: auto;
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
  position: relative;
  z-index: 9999;
  pointer-events: auto;
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