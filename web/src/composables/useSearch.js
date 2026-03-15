// 搜索功能模块
import { ref, computed } from 'vue';
import { useGlobalState } from './useGlobalState';

// 添加历史记录
const addToHistory = (content, type) => {
  try {
    const history = JSON.parse(localStorage.getItem('history') || '[]');
    const now = new Date();
    const record = {
      id: Date.now(),
      date: now.toISOString().split('T')[0],
      time: now.toTimeString().split(' ')[0],
      content,
      type
    };
    history.unshift(record);
    // 限制历史记录数量为100条
    if (history.length > 100) {
      history.pop();
    }
    localStorage.setItem('history', JSON.stringify(history));
  } catch (error) {
    console.error('Failed to add to history:', error);
  }
};

const SEARCH_ENGINES = [
  { name: 'Google', value: 'google' },
  { name: 'Bing', value: 'bing' },
  { name: '百度', value: 'baidu' }
];

export function useSearch() {
  const searchQuery = ref('');
  const { currentEngine, searchHistory } = useGlobalState();
  const isSearchExpanded = ref(false);
  
  // 只显示最近的 5 条搜索记录
  const displayHistory = computed(() => {
    return searchHistory.value.slice(0, 5);
  });

  const toggleEngine = () => {
    const currentIndex = SEARCH_ENGINES.findIndex(engine => engine.value === currentEngine.value);
    const nextIndex = (currentIndex + 1) % SEARCH_ENGINES.length;
    currentEngine.value = SEARCH_ENGINES[nextIndex].value;
  };

  const performSearch = () => {
    if (searchQuery.value.trim()) {
      if (!searchHistory.value.includes(searchQuery.value)) {
        searchHistory.value.unshift(searchQuery.value);
        if (searchHistory.value.length > 5) {
          searchHistory.value = searchHistory.value.slice(0, 5);
        }
      }
      
      // 根据选择的搜索引擎打开搜索页面
      const searchUrls = {
        google: `https://www.google.com/search?q=${encodeURIComponent(searchQuery.value)}`,
        bing: `https://www.bing.com/search?q=${encodeURIComponent(searchQuery.value)}`,
        baidu: `https://www.baidu.com/s?wd=${encodeURIComponent(searchQuery.value)}`
      };
      
      const url = searchUrls[currentEngine.value] || searchUrls.bing;
         window.open(url, '_blank');
         
         // 添加到历史记录
         addToHistory(searchQuery.value, 'search');
         
         // 清空搜索框
         searchQuery.value = '';
         isSearchExpanded.value = false;
       }
  };

  const handleKeyPress = (e) => {
    if (e.key === 'Enter') {
      performSearch();
    }
  };

  const selectHistory = (item) => {
    searchQuery.value = item;
    performSearch();
  };

  // 删除搜索记录
  const deleteHistory = (item) => {
    const index = searchHistory.value.indexOf(item);
    if (index !== -1) {
      searchHistory.value.splice(index, 1);
    }
  };

  const closeSearch = () => {
    isSearchExpanded.value = false;
  };

  const getCurrentEngineName = () => {
    const engine = SEARCH_ENGINES.find(e => e.value === currentEngine.value);
    return engine ? engine.name.charAt(0) : 'G';
  };

  // 获取当前搜索引擎的 URL
  const getSearchUrl = (query) => {
    const searchUrls = {
      google: `https://www.google.com/search?q=${encodeURIComponent(query)}`,
      bing: `https://www.bing.com/search?q=${encodeURIComponent(query)}`,
      baidu: `https://www.baidu.com/s?wd=${encodeURIComponent(query)}`
    };
    return searchUrls[currentEngine.value] || searchUrls.bing;
  };

  return {
    searchQuery,
    searchHistory,
    displayHistory,
    currentEngine,
    isSearchExpanded,
    toggleEngine,
    performSearch,
    handleKeyPress,
    selectHistory,
    deleteHistory,
    closeSearch,
    getCurrentEngineName,
    getSearchUrl
  };
}
