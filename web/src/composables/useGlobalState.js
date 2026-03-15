// 全局状态管理
import { ref, watch } from 'vue';

// 搜索引擎状态
const currentEngine = ref('bing'); // 默认使用 Bing

// 从本地存储读取搜索历史
const loadSearchHistory = () => {
  try {
    const savedHistory = localStorage.getItem('searchHistory');
    return savedHistory ? JSON.parse(savedHistory) : [];
  } catch (error) {
    console.error('Failed to load search history from localStorage:', error);
    return [];
  }
};

// 搜索历史
const searchHistory = ref(loadSearchHistory());

// 监听搜索历史变化，保存到本地存储
watch(searchHistory, (newHistory) => {
  try {
    localStorage.setItem('searchHistory', JSON.stringify(newHistory));
  } catch (error) {
    console.error('Failed to save search history to localStorage:', error);
  }
}, { deep: true });

export function useGlobalState() {
  return {
    currentEngine,
    searchHistory
  };
}
