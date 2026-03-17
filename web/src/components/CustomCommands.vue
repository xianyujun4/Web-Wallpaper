<!-- 自定义指令管理组件 -->
<script setup>
import { ref, onMounted } from 'vue';
// 直接使用 useCommand 里统一的存储 API，不再自己维护一套加载/保存逻辑
import { useCommand, readCommandsFromStorage, writeCommandsToStorage, customCommands } from '../composables/useCommand';

const { executeCommand } = useCommand();

const emit = defineEmits(['close']);

// 直接使用 useCommand 里的共享 ref，保证数据始终同步
// 这里用本地 ref 做 UI 展示即可，挂载时从共享源同步一次
const localCommands = ref([]);

// 新指令表单
const newCommand = ref({
  name: '',
  description: '',
  commands: ''
});

// 编辑模式
const editMode = ref(false);
const editingIndex = ref(null);

// 挂载时从 localStorage（通过 useCommand 统一接口）读取
onMounted(() => {
  localCommands.value = readCommandsFromStorage();
});

// 保存到 localStorage，并同步共享 ref
const persist = () => {
  writeCommandsToStorage(localCommands.value);
};

// 添加 / 保存编辑
const addCommand = () => {
  const name = newCommand.value.name.trim();
  const commands = newCommand.value.commands.trim();

  if (!name || !commands) {
    alert('请填写指令名称和命令内容');
    return;
  }

  // 名称不能与系统命令重名（可选保护）
  const reserved = ['open', 'run', 'del', 'copy', 'move', 'rename'];
  if (!editMode.value && reserved.includes(name.toLowerCase())) {
    alert(`"${name}" 是系统保留命令名，请换一个名称`);
    return;
  }

  const entry = {
    name: name.toLowerCase(),          // 统一小写，避免大小写匹配问题
    description: newCommand.value.description.trim(),
    commands
  };

  if (editMode.value) {
    localCommands.value[editingIndex.value] = entry;
    editMode.value = false;
    editingIndex.value = null;
  } else {
    // 检查重名
    if (localCommands.value.some(c => c.name === entry.name)) {
      alert(`指令 "${entry.name}" 已存在，请先删除或直接编辑`);
      return;
    }
    localCommands.value.push(entry);
  }

  // 清空表单
  newCommand.value = { name: '', description: '', commands: '' };

  persist();
  alert('保存成功！');
};

// 取消编辑
const cancelEdit = () => {
  editMode.value = false;
  editingIndex.value = null;
  newCommand.value = { name: '', description: '', commands: '' };
};

// 编辑指令
const editCommand = (index) => {
  const cmd = localCommands.value[index];
  newCommand.value = {
    name: cmd.name,
    description: cmd.description,
    commands: cmd.commands
  };
  editMode.value = true;
  editingIndex.value = index;
};

// 删除指令
const deleteCommand = (index) => {
  if (confirm('确定要删除这个指令吗？')) {
    localCommands.value.splice(index, 1);
    persist();
  }
};

// 运行指令（直接调用共享的 executeCommand，数据来自最新 localStorage）
const runCommand = (command) => {
  const cmdStr = `#${command.name}`;
  const success = executeCommand(cmdStr);
  if (success) {
    console.log('运行指令:', cmdStr);
  } else {
    alert(`指令 "${command.name}" 执行失败，请检查命令内容是否正确`);
  }
};

const handleClose = () => {
  emit('close');
};
</script>

<template>
  <div class="custom-commands-overlay" @click.stop>
    <div class="custom-commands-window">
      <div class="custom-commands-header">
        <h3>自定义指令管理</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>

      <!-- 新指令表单 -->
      <div class="custom-commands-form">
        <h4>{{ editMode ? '编辑指令' : '添加新指令' }}</h4>
        <div class="form-group">
          <label>指令名称：</label>
          <input
            type="text"
            v-model="newCommand.name"
            class="form-input"
            placeholder="例如：startup（字母/数字，不含空格）"
            :disabled="editMode"
          />
        </div>
        <div class="form-group">
          <label>指令描述：</label>
          <input
            type="text"
            v-model="newCommand.description"
            class="form-input"
            placeholder="例如：启动常用应用"
          />
        </div>
        <div class="form-group">
          <label>命令内容：</label>
          <textarea
            v-model="newCommand.commands"
            class="form-textarea"
            placeholder="#open D:/apps/app1.exe&#10;#run D:/apps/app2.exe"
            rows="4"
          ></textarea>
          <div class="form-tip">
            每行一个命令，使用 # 前缀。例如：<br>
            <code>#open D:/tools/notepad.exe</code><br>
            <code>#run D:/games/game.exe</code>
          </div>
        </div>
        <div class="form-actions">
          <button class="add-button" @click="addCommand">
            {{ editMode ? '保存修改' : '添加指令' }}
          </button>
          <button v-if="editMode" class="cancel-edit-button" @click="cancelEdit">
            取消编辑
          </button>
        </div>
      </div>

      <!-- 自定义指令列表 -->
      <div class="custom-commands-list">
        <h4>已添加的指令（共 {{ localCommands.length }} 条）</h4>
        <div v-if="localCommands.length === 0" class="empty-list">
          暂无自定义指令，点击上方添加
        </div>
        <div v-else class="commands-grid">
          <div
            v-for="(command, index) in localCommands"
            :key="command.name"
            class="command-item"
            :class="{ editing: editMode && editingIndex === index }"
          >
            <div class="command-info">
              <div class="command-name">
                <span class="name-tag">#{{ command.name }}</span>
                <span class="command-description">{{ command.description }}</span>
              </div>
              <div class="command-content">{{ command.commands }}</div>
            </div>
            <div class="command-actions">
              <button class="run-button" @click="runCommand(command)" title="立即运行">▶ 运行</button>
              <button class="edit-button" @click="editCommand(index)" title="编辑">✎ 编辑</button>
              <button class="delete-button" @click="deleteCommand(index)" title="删除">✕ 删除</button>
            </div>
          </div>
        </div>
      </div>

      <!-- 底部按钮 -->
      <div class="custom-commands-footer">
        <button class="cancel-button" @click.stop="handleClose">关闭</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.custom-commands-overlay {
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
  width: 100vw;
  height: 100vh;
  overflow: hidden;
  pointer-events: auto;
}

.custom-commands-window {
  background-color: rgba(0, 0, 0, 0.8);
  backdrop-filter: blur(10px);
  -webkit-backdrop-filter: blur(10px);
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4);
  padding: 20px;
  width: 90%;
  max-width: 800px;
  height: 95%;
  max-height: 900px;
  color: rgba(255, 255, 255, 0.9);
  margin: auto;
  position: relative;
  z-index: 10000;
  pointer-events: auto;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.custom-commands-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
  flex-shrink: 0;
}

.custom-commands-header h3 {
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

.custom-commands-form {
  margin-bottom: 20px;
  padding-bottom: 20px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
  flex-shrink: 0;
}

.custom-commands-form h4 {
  margin: 0 0 15px 0;
  font-size: 16px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
}

.form-group {
  margin-bottom: 12px;
}

.form-group label {
  display: block;
  margin-bottom: 6px;
  font-size: 13px;
  color: rgba(255, 255, 255, 0.7);
}

.form-input {
  width: 100%;
  padding: 8px 12px;
  background-color: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 14px;
  outline: none;
  box-sizing: border-box;
  transition: border-color 0.2s;
}

.form-input:focus {
  border-color: rgba(76, 175, 80, 0.5);
}

.form-input:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.form-textarea {
  width: 100%;
  padding: 8px 12px;
  background-color: rgba(255, 255, 255, 0.05);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.9);
  font-size: 13px;
  font-family: monospace;
  outline: none;
  box-sizing: border-box;
  resize: vertical;
  transition: border-color 0.2s;
}

.form-textarea:focus {
  border-color: rgba(76, 175, 80, 0.5);
}

.form-tip {
  margin-top: 6px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.5);
  line-height: 1.6;
}

.form-tip code {
  background: rgba(255, 255, 255, 0.08);
  padding: 1px 5px;
  border-radius: 3px;
  font-family: monospace;
}

.form-actions {
  display: flex;
  gap: 10px;
  margin-top: 12px;
}

.add-button {
  padding: 9px 20px;
  background-color: rgba(76, 175, 80, 0.2);
  border: 1px solid rgba(76, 175, 80, 0.3);
  border-radius: 6px;
  color: rgba(76, 175, 80, 0.9);
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.add-button:hover {
  background-color: rgba(76, 175, 80, 0.3);
  color: rgba(76, 175, 80, 1);
}

.cancel-edit-button {
  padding: 9px 20px;
  background-color: rgba(255, 255, 255, 0.08);
  border: 1px solid rgba(255, 255, 255, 0.15);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.6);
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.cancel-edit-button:hover {
  background-color: rgba(255, 255, 255, 0.12);
  color: rgba(255, 255, 255, 0.9);
}

.custom-commands-list {
  flex: 1;
  overflow-y: auto;
  padding-right: 2px;
}

.custom-commands-list h4 {
  margin: 0 0 12px 0;
  font-size: 15px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
  flex-shrink: 0;
}

.empty-list {
  text-align: center;
  color: rgba(255, 255, 255, 0.4);
  padding: 40px 0;
  font-size: 14px;
}

.commands-grid {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.command-item {
  background-color: rgba(255, 255, 255, 0.05);
  border: 1px solid transparent;
  border-radius: 8px;
  padding: 12px 15px;
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  transition: all 0.2s ease;
  gap: 12px;
}

.command-item:hover {
  background-color: rgba(255, 255, 255, 0.08);
}

.command-item.editing {
  border-color: rgba(76, 175, 80, 0.4);
  background-color: rgba(76, 175, 80, 0.06);
}

.command-info {
  flex: 1;
  min-width: 0;
}

.command-name {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 8px;
  flex-wrap: wrap;
}

.name-tag {
  font-size: 13px;
  font-weight: 600;
  color: rgba(76, 175, 80, 0.9);
  background: rgba(76, 175, 80, 0.1);
  border: 1px solid rgba(76, 175, 80, 0.2);
  padding: 2px 8px;
  border-radius: 4px;
  font-family: monospace;
}

.command-description {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.5);
}

.command-content {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.7);
  background-color: rgba(0, 0, 0, 0.3);
  padding: 7px 10px;
  border-radius: 4px;
  white-space: pre-wrap;
  word-break: break-all;
  font-family: monospace;
  line-height: 1.6;
}

.command-actions {
  display: flex;
  flex-direction: column;
  gap: 6px;
  flex-shrink: 0;
}

.run-button, .edit-button, .delete-button {
  padding: 5px 11px;
  border: none;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.2s ease;
  white-space: nowrap;
}

.run-button {
  background-color: rgba(76, 175, 80, 0.2);
  color: rgba(76, 175, 80, 0.9);
}

.run-button:hover {
  background-color: rgba(76, 175, 80, 0.35);
  color: rgba(76, 175, 80, 1);
}

.edit-button {
  background-color: rgba(33, 150, 243, 0.2);
  color: rgba(33, 150, 243, 0.9);
}

.edit-button:hover {
  background-color: rgba(33, 150, 243, 0.35);
  color: rgba(33, 150, 243, 1);
}

.delete-button {
  background-color: rgba(244, 67, 54, 0.15);
  color: rgba(244, 67, 54, 0.8);
}

.delete-button:hover {
  background-color: rgba(244, 67, 54, 0.3);
  color: rgba(244, 67, 54, 1);
}

.custom-commands-footer {
  display: flex;
  justify-content: flex-end;
  padding-top: 15px;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  margin-top: auto;
  flex-shrink: 0;
}

.cancel-button {
  padding: 9px 20px;
  background-color: rgba(255, 255, 255, 0.1);
  border: none;
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.7);
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.cancel-button:hover {
  background-color: rgba(255, 255, 255, 0.15);
  color: rgba(255, 255, 255, 0.9);
}

.custom-commands-list::-webkit-scrollbar {
  width: 6px;
}

.custom-commands-list::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.03);
  border-radius: 3px;
}

.custom-commands-list::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.15);
  border-radius: 3px;
}

.custom-commands-list::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.25);
}

@media (max-width: 768px) {
  .custom-commands-window {
    width: 95%;
    height: 90%;
    padding: 15px;
  }

  .command-item {
    flex-direction: column;
    align-items: stretch;
  }

  .command-actions {
    flex-direction: row;
    margin-top: 8px;
  }

  .run-button, .edit-button, .delete-button {
    flex: 1;
    text-align: center;
  }
}
</style>