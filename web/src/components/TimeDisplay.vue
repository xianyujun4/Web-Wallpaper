<!-- 时间显示模块 -->
<script setup>
import { useTime } from '../composables/useTime';

const { currentTime, currentDate } = useTime();

defineProps({
  blurred: {
    type: Boolean,
    default: false
  }
});
</script>

<template>
  <div class="time-content" :class="{ 'blurred': blurred }">
    <div class="time">{{ currentTime }}</div>
    <div class="date">{{ currentDate }}</div>
  </div>
</template>

<style scoped>
.time-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  transition: filter 0.3s ease;
}

.time-content.blurred {
  filter: blur(5px);
}

.time {
  font-size: 72px;
  font-weight: 300;
  color: #333;
  margin-bottom: 5px;
}

.date {
  font-size: 24px;
  font-weight: 300;
  color: #666;
}
</style>
