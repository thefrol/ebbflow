<template>
  <div class="pulse-form">
    <label>
      Полив каждые, минут
      <input v-model.number="model.pulse_interval_min" type="number" min="1" max="1440" required />
    </label>

    <label>
      Длительность полива, секунд
      <input v-model.number="model.pulse_duration_sec" type="number" min="1" max="600" required />
    </label>

    <div v-if="status" class="pulse-status">
      <div class="row">
        <span class="label">Следующий полив</span>
        <span v-if="status.manual_pulse_active" class="value now">идёт сейчас</span>
        <span v-else-if="status.next_pulse_time" class="value">
          {{ status.next_pulse_today ? 'сегодня' : 'завтра' }} в {{ status.next_pulse_time }}
        </span>
        <span v-else class="value">—</span>
      </div>
    </div>

    <div v-if="status && status.pulse_times.length > 0" class="pulse-list">
      <div class="label">Ближайшие поливы</div>
      <ul>
        <li v-for="(t, i) in status.pulse_times" :key="i" :class="{ past: t.past }">
          <span>{{ t.time }}</span>
          <span>{{ formatDayOffset(t.day_offset) }}</span>
        </li>
      </ul>
    </div>

    <button type="button" class="secondary" :disabled="busy" @click="emit('water')">
      Полить сейчас
    </button>
  </div>
</template>

<script setup lang="ts">
import type { LampSettings, LampStatus } from '../types.ts'

defineProps<{
  status?: LampStatus
  busy: boolean
}>()

const emit = defineEmits<{
  water: []
}>()

const model = defineModel<LampSettings>({ required: true })

function formatDayOffset(offset: number): string {
  const map: Record<number, string> = {
    [-2]: 'позавчера',
    [-1]: 'вчера',
    0: 'сегодня',
    1: 'завтра',
    2: 'послезавтра',
  }
  if (offset in map) return map[offset]
  return offset > 0 ? `через ${offset} дн.` : `${-offset} дн. назад`
}
</script>

<style scoped>
.pulse-form {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}
label {
  font-weight: 500;
  color: var(--text-muted);
}
input[type='number'] {
  width: 100%;
  padding: 0.5rem;
  font-size: 1rem;
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  background: var(--input-bg);
  color: var(--text);
  box-sizing: border-box;
}
.pulse-status,
.pulse-list {
  background: var(--surface-alt);
  border-radius: 0.5rem;
  padding: 0.75rem;
  font-size: 0.95rem;
}
.pulse-status .row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 0.5rem;
}
.pulse-status .label,
.pulse-list .label {
  opacity: 0.7;
}
.pulse-status .value {
  font-weight: 600;
}
.pulse-status .value.now {
  color: var(--ok-text);
}
.pulse-list ul {
  list-style: none;
  margin: 0.5rem 0 0;
  padding: 0;
}
.pulse-list li {
  display: flex;
  justify-content: space-between;
  padding: 0.25rem 0;
}
.pulse-list li.past {
  opacity: 0.55;
}
</style>
