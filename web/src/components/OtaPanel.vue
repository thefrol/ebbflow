<template>
  <div class="ota-panel">
    <div class="meta">
      <div>Текущая версия: {{ status.current_version }}</div>
      <div v-if="status.available_version">Доступна: {{ status.available_version }}</div>
    </div>

    <StatusMessage :text="stateText" :kind="stateKind" />

    <div class="buttons">
      <button type="button" class="secondary" :disabled="busy" @click="emit('check')">
        {{ busy && status.state === 'checking' ? 'Проверяю…' : 'Проверить обновление' }}
      </button>
      <button type="button" :disabled="!canStart || busy" @click="emit('start')">
        {{ busy && status.state === 'downloading' ? 'Скачиваю…' : 'Скачать обновление' }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { UpdateStatus } from '../types.ts'
import StatusMessage from './StatusMessage.vue'

const props = defineProps<{
  status: UpdateStatus
  busy: boolean
}>()

const emit = defineEmits<{
  check: []
  start: []
}>()

const stateMap: Record<string, string> = {
  idle: 'Нажмите «Проверить обновление»',
  checking: 'Проверяю обновление…',
  update_available: 'Доступна новая версия',
  downloading: 'Скачиваю и устанавливаю…',
  reboot_pending: 'Обновление установлено, перезагрузка…',
  up_to_date: 'Установлена актуальная версия',
}

const stateText = computed(() => {
  if (props.status.state === 'error') return `Ошибка: ${props.status.error || 'неизвестно'}`
  return stateMap[props.status.state] || props.status.state
})

const stateKind = computed(() => {
  switch (props.status.state) {
    case 'update_available':
    case 'up_to_date':
    case 'reboot_pending':
      return 'ok'
    case 'checking':
    case 'downloading':
      return 'info'
    case 'error':
      return 'err'
    default:
      return ''
  }
})

const canStart = computed(() => props.status.state === 'update_available')
</script>

<style scoped>
.ota-panel {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}
.meta {
  font-size: 0.9rem;
  color: var(--text-muted);
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}
.buttons {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}
</style>
