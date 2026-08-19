<template>
  <div class="schedule-form">
    <label>
      Включение
      <input v-model="model.on" type="time" required />
    </label>

    <label>
      Выключение
      <input v-model="model.off" type="time" required />
    </label>

    <p class="hint">
      Расписание работает через полночь: если «включение» позже «выключения», лампа горит ночью.
    </p>
  </div>
</template>

<script setup lang="ts">
import type { LampSettings } from '../types.ts'

const model = defineModel<LampSettings>({ required: true })
</script>

<style scoped>
.schedule-form {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}
label {
  font-weight: 500;
  color: var(--text-muted);
}
input[type='time'] {
  width: 100%;
  padding: 0.5rem;
  font-size: 1rem;
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  background: var(--input-bg);
  color: var(--text);
  box-sizing: border-box;
}
.hint {
  font-size: 0.85rem;
  color: var(--text-muted);
  margin: 0.25rem 0 0;
  line-height: 1.4;
}
</style>
