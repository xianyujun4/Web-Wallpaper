<!-- 主页编辑组件 -->
<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue';

// 声明组件可以接收的属性
const props = defineProps({
  apps: {
    type: Array,
    required: true
  }
});

// 声明组件可以发射的事件
const emit = defineEmits(['close', 'go-to-home', 'update:apps', 'update:wallpaper']);

// 本地应用列表，用于编辑
const localApps = ref([]);
// 壁纸类型
const wallpaperType = ref('image'); // image, video, transparent
// 壁纸路径
const wallpaperPath = ref('/background.jpg');
// 视频壁纸路径
const videoPath = ref('');
// 选中索引
const selectedIndex = ref(null);

// 初始化本地应用列表 - 已移至下方完整的onMounted函数

// 从 Base64 数据创建 blob URL
const createBlobUrlFromBase64 = (base64Data) => {
  const arr = base64Data.split(',');
  const mime = arr[0].match(/:(.*?);/)[1];
  const bstr = atob(arr[1]);
  let n = bstr.length;
  const u8arr = new Uint8Array(n);
  while (n--) {
    u8arr[n] = bstr.charCodeAt(n);
  }
  return URL.createObjectURL(new Blob([u8arr], { type: mime }));
};

// 加载壁纸设置
const loadWallpaperSettings = async () => {
  try {
    const savedWallpaper = localStorage.getItem('wallpaper');
    if (savedWallpaper) {
      const wallpaper = JSON.parse(savedWallpaper);
      wallpaperType.value = wallpaper.type || 'image';
      wallpaperPath.value = wallpaper.path || '/background.jpg';
      
      // 如果是视频壁纸，从 IndexedDB 读取视频数据
      if (wallpaper.type === 'video') {
        const videoData = await getVideoFromDB();
        if (videoData) {
          // 从 Base64 数据创建 blob URL
          videoPath.value = createBlobUrlFromBase64(videoData);
        } else {
          videoPath.value = '';
        }
      } else {
        videoPath.value = '';
      }
    }
  } catch (error) {
    console.error('Failed to load wallpaper settings:', error);
  }
};

// IndexedDB 操作
const dbName = 'wallpaperDB';
const dbVersion = 1;

// 打开数据库
const openDB = () => {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(dbName, dbVersion);
    
    request.onerror = () => reject('Failed to open database');
    
    request.onsuccess = () => resolve(request.result);
    
    request.onupgradeneeded = (event) => {
      const db = event.target.result;
      if (!db.objectStoreNames.contains('videos')) {
        db.createObjectStore('videos', { keyPath: 'id' });
      }
    };
  });
};

// 保存视频到 IndexedDB
const saveVideoToDB = async (videoData) => {
  try {
    const db = await openDB();
    const transaction = db.transaction(['videos'], 'readwrite');
    const store = transaction.objectStore('videos');
    
    const videoId = 'currentVideo';
    await store.put({ id: videoId, data: videoData });
    return videoId;
  } catch (error) {
    console.error('Failed to save video to DB:', error);
    throw error;
  }
};

// 从 IndexedDB 读取视频
const getVideoFromDB = async () => {
  try {
    const db = await openDB();
    const transaction = db.transaction(['videos'], 'readonly');
    const store = transaction.objectStore('videos');
    
    const request = store.get('currentVideo');
    return new Promise((resolve, reject) => {
      request.onsuccess = () => resolve(request.result?.data || null);
      request.onerror = () => reject('Failed to get video from DB');
    });
  } catch (error) {
    console.error('Failed to get video from DB:', error);
    return null;
  }
};

// 将文件转为 Base64
const fileToBase64 = (file) => {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = () => reject('Failed to convert file to base64');
    reader.readAsDataURL(file);
  });
};

// 保存壁纸设置
const saveWallpaperSettings = async () => {
  try {
    let videoId = null;
    
    const wallpaper = {
      type: wallpaperType.value,
      path: wallpaperPath.value,
      videoPath: videoPath.value
    };
    localStorage.setItem('wallpaper', JSON.stringify(wallpaper));
    // 通知父组件更新壁纸
    emit('update:wallpaper', wallpaper);
  } catch (error) {
    console.error('Failed to save wallpaper settings:', error);
  }
};

// 组件卸载时释放临时URL
onUnmounted(() => {
  window.removeEventListener('keydown', handleKeyDown);
  // 释放临时URL
  if (wallpaperPath.value.startsWith('blob:')) {
    URL.revokeObjectURL(wallpaperPath.value);
  }
  if (videoPath.value.startsWith('blob:')) {
    URL.revokeObjectURL(videoPath.value);
  }
});

// 处理壁纸类型变化
const handleWallpaperTypeChange = (type) => {
  wallpaperType.value = type;
  saveWallpaperSettings();
};



// 处理文件路径变化
const handleWallpaperPathChange = (path) => {
  wallpaperPath.value = path;
  saveWallpaperSettings();
};

// 处理视频路径变化
const handleVideoPathChange = (path) => {
  videoPath.value = path;
  saveWallpaperSettings();
};

// 处理视频文件选择
const handleVideoFileSelect = async (event) => {
  const file = event.target.files[0];
  if (file) {
    try {
      // 将文件转为 Base64 并保存到 IndexedDB
      const base64Data = await fileToBase64(file);
      await saveVideoToDB(base64Data);
      
      // 使用URL.createObjectURL创建临时URL用于预览
      const blobUrl = URL.createObjectURL(file);
      handleVideoPathChange(blobUrl);
    } catch (error) {
      console.error('Failed to handle video file:', error);
      alert('视频文件处理失败，请尝试较小的视频文件');
    }
  }
};

// 处理图片文件选择
const handleImageFileSelect = (event) => {
  const file = event.target.files[0];
  if (file) {
    // 使用URL.createObjectURL创建临时URL
    const blobUrl = URL.createObjectURL(file);
    handleWallpaperPathChange(blobUrl);
  }
};

// 处理选中应用
const handleAppClick = (event, index) => {
  event.stopPropagation();
  selectedIndex.value = index;
};

// 处理键盘事件
const handleKeyDown = (event) => {
  if (selectedIndex.value === null) return;
  
  if (event.key === 'ArrowUp' && selectedIndex.value > 0) {
    // 向上移动
    const movedApp = localApps.value[selectedIndex.value];
    localApps.value.splice(selectedIndex.value, 1);
    localApps.value.splice(selectedIndex.value - 1, 0, movedApp);
    selectedIndex.value--;
  } else if (event.key === 'ArrowDown' && selectedIndex.value < localApps.value.length - 1) {
    // 向下移动
    const movedApp = localApps.value[selectedIndex.value];
    localApps.value.splice(selectedIndex.value, 1);
    localApps.value.splice(selectedIndex.value + 1, 0, movedApp);
    selectedIndex.value++;
  }
};

// 组件挂载时添加键盘事件监听
onMounted(() => {
  // 深拷贝应用列表
  localApps.value = JSON.parse(JSON.stringify(props.apps));
  // 加载壁纸设置
  loadWallpaperSettings();
  // 添加键盘事件监听
  window.addEventListener('keydown', handleKeyDown);
});



// 保存应用顺序
const saveAppOrder = () => {
  emit('update:apps', localApps.value);
};

// 处理关闭窗口
const handleClose = () => {
  saveAppOrder();
  emit('close');
};

// 处理回到主页
const handleGoToHome = () => {
  saveAppOrder();
  emit('go-to-home');
};
</script>

<template>
  <div class="home-edit-overlay" @click.stop>
    <div class="home-edit-window">
      <div class="home-edit-header">
        <h3>主页编辑</h3>
        <button class="close-button" @click="handleClose">×</button>
      </div>
      
      <!-- 壁纸设置 -->
      <div class="home-edit-section">
        <h4>壁纸设置</h4>
        <div class="wallpaper-settings">
          <!-- 壁纸类型选择 -->
          <div class="wallpaper-type-selector">
            <button 
              class="type-button" 
              :class="{ active: wallpaperType === 'image' }"
              @click="handleWallpaperTypeChange('image')"
            >
              图片壁纸
            </button>
            <button 
              class="type-button" 
              :class="{ active: wallpaperType === 'video' }"
              @click="handleWallpaperTypeChange('video')"
            >
              视频动态壁纸
            </button>
            <button 
              class="type-button" 
              :class="{ active: wallpaperType === 'transparent' }"
              @click="handleWallpaperTypeChange('transparent')"
            >
              透明
            </button>
          </div>
          
          <!-- 图片壁纸设置 -->
          <div v-if="wallpaperType === 'image'" class="wallpaper-option">
            <label>图片路径：</label>
            <input 
              type="text" 
              v-model="wallpaperPath" 
              class="path-input"
              @input="handleWallpaperPathChange(wallpaperPath)"
              placeholder="例如：D:/wallpaper/image.jpg"
            />
            <input 
              type="file" 
              accept="image/*" 
              class="file-input"
              @change="handleImageFileSelect"
            />
            <div class="wallpaper-preview" v-if="wallpaperPath">
              <img :src="wallpaperPath" alt="壁纸预览" />
            </div>
          </div>
          
          <!-- 视频壁纸设置 -->
          <div v-if="wallpaperType === 'video'" class="wallpaper-option">
            <label>视频路径：</label>
            <input 
              type="text" 
              v-model="videoPath" 
              class="path-input"
              @input="handleVideoPathChange(videoPath)"
              placeholder="例如：D:/wallpaper/video.mp4"
            />
            <input 
              type="file" 
              accept="video/*" 
              class="file-input"
              @change="handleVideoFileSelect"
            />
            <div class="wallpaper-preview" v-if="videoPath">
              <video 
                :src="videoPath" 
                controls 
                muted 
                autoplay 
                loop
                @error="() => alert('视频加载失败，请检查路径是否正确，或尝试使用文件选择按钮')"
              ></video>
              <div class="video-tip">
                提示：使用文件选择按钮可以直接选择本地视频文件
              </div>
            </div>
          </div>
          
          <!-- 透明壁纸设置 -->
          <div v-if="wallpaperType === 'transparent'" class="wallpaper-option">
            <p>选择透明壁纸后，背景将变为透明，显示桌面背景。</p>
          </div>
        </div>
      </div>
      
      <!-- APP按钮顺序调整 -->
      <div class="home-edit-section">
        <h4>APP按钮顺序调整</h4>
        <p class="section-desc">点击选中应用，然后按键盘上下箭头调整位置</p>
        <div class="app-order-list">
          <div 
            v-for="(app, index) in localApps" 
            :key="app.id"
            class="app-order-item"
            :class="{ 'selected': selectedIndex === index }"
            @click="handleAppClick($event, index)"
          >
            <div class="app-info">
              <div class="app-icon">{{ app.icon }}</div>
              <div class="app-details">
                <div class="app-name">{{ app.name }}</div>
                <div class="app-description">{{ app.description }}</div>
              </div>
            </div>
            <div class="select-indicator">
              <span v-if="selectedIndex === index">▼</span>
            </div>
          </div>
        </div>
      </div>
      
      <!-- 底部按钮 -->
      <div class="home-edit-footer">
        <button class="cancel-button" @click.stop="handleClose">取消</button>
        <button class="save-button" @click.stop="handleGoToHome">保存并返回</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 主页编辑窗口覆盖层 */
.home-edit-overlay {
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

/* 主页编辑窗口 */
.home-edit-window {
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

/* 主页编辑窗口头部 */
.home-edit-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.home-edit-header h3 {
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

/* 主页编辑内容 */
.home-edit-section {
  margin-bottom: 25px;
  padding-bottom: 20px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.home-edit-section:last-child {
  border-bottom: none;
  margin-bottom: 0;
  padding-bottom: 0;
}

.home-edit-section h4 {
  margin: 0 0 15px 0;
  font-size: 16px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
}

.section-desc {
  margin: 0 0 15px 0;
  font-size: 14px;
  color: rgba(255, 255, 255, 0.6);
}

/* 壁纸设置 */
.wallpaper-settings {
  margin-top: 10px;
}

.wallpaper-type-selector {
  display: flex;
  gap: 10px;
  margin-bottom: 20px;
}

.type-button {
  padding: 8px 16px;
  background-color: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  color: rgba(255, 255, 255, 0.7);
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.type-button:hover {
  background-color: rgba(255, 255, 255, 0.15);
  color: rgba(255, 255, 255, 0.9);
}

.type-button.active {
  background-color: rgba(76, 175, 80, 0.2);
  border-color: rgba(76, 175, 80, 0.4);
  color: rgba(76, 175, 80, 0.9);
}

.wallpaper-option {
  margin-top: 15px;
}

.wallpaper-option label {
  display: block;
  margin-bottom: 8px;
  font-size: 14px;
  color: rgba(255, 255, 255, 0.7);
}

.path-input {
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

.path-input::placeholder {
  color: rgba(255, 255, 255, 0.5);
}

.file-input {
  margin-bottom: 15px;
  color: rgba(255, 255, 255, 0.7);
}

.wallpaper-preview {
  margin-top: 15px;
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 8px;
  overflow: hidden;
  max-width: 300px;
  max-height: 200px;
}

.wallpaper-preview img,
.wallpaper-preview video {
  width: 100%;
  height: auto;
  display: block;
}

.video-tip {
  margin-top: 10px;
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
  text-align: center;
  background-color: rgba(255, 255, 255, 0.05);
  padding: 8px;
  border-radius: 4px;
}

/* APP按钮顺序调整 */
.app-order-list {
  margin-top: 15px;
  max-height: 300px;
  overflow-y: auto;
}

.app-order-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 15px;
  background-color: rgba(255, 255, 255, 0.05);
  border-radius: 8px;
  margin-bottom: 10px;
  transition: all 0.2s ease;
  cursor: pointer;
}

.app-order-item:hover {
  background-color: rgba(255, 255, 255, 0.08);
}

.app-order-item.selected {
  background-color: rgba(76, 175, 80, 0.2);
  border: 2px solid rgba(76, 175, 80, 0.4);
}

.app-info {
  display: flex;
  align-items: center;
  flex: 1;
}

.app-icon {
  font-size: 24px;
  margin-right: 15px;
  width: 40px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: rgba(255, 255, 255, 0.1);
  border-radius: 6px;
}

.app-details {
  flex: 1;
}

.app-name {
  font-size: 14px;
  font-weight: 500;
  color: rgba(255, 255, 255, 0.9);
  margin-bottom: 4px;
}

.app-description {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
}

.select-indicator {
  width: 30px;
  height: 30px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: rgba(76, 175, 80, 0.9);
  font-size: 16px;
  font-weight: bold;
}

/* 主页编辑窗口底部 */
.home-edit-footer {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  padding-top: 20px;
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  margin-top: auto;
}

/* 底部按钮 */
.cancel-button, .save-button {
  padding: 10px 20px;
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

/* 滚动条样式 */
.app-order-list::-webkit-scrollbar {
  width: 8px;
}

.app-order-list::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
}

.app-order-list::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.2);
  border-radius: 4px;
}

.app-order-list::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.3);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .home-edit-window {
    width: 95%;
    height: 85%;
    padding: 15px;
  }
  
  .wallpaper-type-selector {
    flex-direction: column;
  }
  
  .type-button {
    width: 100%;
  }
  
  .app-order-item {
    padding: 12px;
  }
  
  .app-icon {
    font-size: 20px;
    width: 36px;
    height: 36px;
    margin-right: 12px;
  }
  
  .app-name {
    font-size: 13px;
  }
  
  .app-description {
    font-size: 11px;
  }
  
  .cancel-button, .save-button {
    padding: 8px 16px;
    font-size: 13px;
  }
}
</style>