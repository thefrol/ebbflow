<template>
  <Card>
    <div class="device-head">
      <span class="device-name">
        {{ info?.name || base || 'это устройство' }}
        <span v-if="isSelf" class="self-badge">эта лампа</span>
      </span>
      <span v-if="info" class="device-meta">v{{ info.version }} · {{ info.chip }}</span>
    </div>

    <div v-if="unavailable" class="device-status">
      недоступно
      <button type="button" class="secondary" @click="load">повторить</button>
    </div>
    <div v-else-if="loading" class="device-status">загрузка…</div>

    <template v-else>
      <form @submit.prevent="onSubmit">
        <div class="form-group">
          <label>
            Имя устройства
            <input v-model="settings.name" type="text" maxlength="31" required />
          </label>
        </div>

        <div class="form-group">
          <label>
            Режим
            <select v-model="settings.mode">
              <option value="schedule">Расписание (свет)</option>
              <option value="pulse">Импульсы (полив)</option>
            </select>
          </label>
        </div>

        <ScheduleForm v-if="settings.mode === 'schedule'" v-model="settings" />
        <PulseForm v-else v-model="settings" :status="status" :busy="busy" @water="onWater" />

        <div class="form-group">
          <label>
            GPIO лампы
            <input v-model.number="settings.gpio" type="number" min="0" max="21" required />
          </label>
        </div>

        <div class="form-group row">
          <input v-model="settings.enabled" type="checkbox" />
          <label>Расписание активно</label>
        </div>

        <button type="submit" :disabled="saving">
          {{ saving ? 'Сохраняю…' : 'Сохранить' }}
        </button>
        <StatusMessage :text="saveMessage" :kind="saveKind" />
      </form>

      <details class="ota-details">
        <summary>Обновление прошивки</summary>
        <OtaPanel :status="updateStatus" :busy="otaBusy" @check="onCheckUpdate" @start="onStartUpdate" />
      </details>
    </template>
  </Card>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted, ref, watch } from 'vue'
import Card from './Card.vue'
import StatusMessage from './StatusMessage.vue'
import ScheduleForm from './ScheduleForm.vue'
import PulseForm from './PulseForm.vue'
import OtaPanel from './OtaPanel.vue'
import {
  getInfo,
  getSettings,
  saveSettings,
  getStatus,
  getUpdateStatus,
  waterNow,
  checkUpdate,
  startUpdate,
} from '../api.ts'
import type { LampInfo, LampSettings, LampStatus, UpdateStatus } from '../types.ts'

const props = defineProps<{
  base: string // '' — устройство, отдавшее страницу; 'http://<ip>' — сосед
  isSelf?: boolean
}>()

const loading = ref(true)
const unavailable = ref(false)
const saving = ref(false)
const busy = ref(false)
const otaBusy = ref(false)
const saveMessage = ref('')
const saveKind = ref<'ok' | 'err' | 'info' | ''>('')

const info = ref<LampInfo | null>(null)
const settings = ref<LampSettings>({
  mode: 'schedule',
  name: '',
  on: '08:00',
  off: '20:00',
  pulse_interval_min: 60,
  pulse_duration_sec: 10,
  gpio: 0,
  enabled: false,
})
const status = ref<LampStatus>()
const updateStatus = ref<UpdateStatus>({
  current_version: '',
  available_version: '',
  state: 'idle',
  error: '',
})

async function load() {
  loading.value = true
  unavailable.value = false
  try {
    const [i, s] = await Promise.all([getInfo(props.base), getSettings(props.base)])
    info.value = i
    settings.value = s
  } catch (e) {
    unavailable.value = true
  } finally {
    loading.value = false
  }
}

async function updateStatusInfo() {
  if (unavailable.value) return
  try {
    status.value = await getStatus(props.base)
  } catch (e) {
    // молча игнорируем — сосед мог уйти из сети, карточку уберёт App
  }
}

async function updateOtaInfo() {
  if (unavailable.value) return
  try {
    updateStatus.value = await getUpdateStatus(props.base)
  } catch (e) {
    // молча игнорируем
  }
}

function showSave(text: string, kind: 'ok' | 'err' | 'info') {
  saveMessage.value = text
  saveKind.value = kind
  if (kind !== 'err') {
    setTimeout(() => {
      saveMessage.value = ''
      saveKind.value = ''
    }, 3000)
  }
}

async function onSubmit() {
  saving.value = true
  try {
    await saveSettings(settings.value, props.base)
    showSave('сохранено', 'ok')
    await updateStatusInfo()
  } catch (e) {
    showSave(e instanceof Error ? e.message : 'ошибка сохранения', 'err')
  } finally {
    saving.value = false
  }
}

async function onWater() {
  busy.value = true
  try {
    status.value = await waterNow(props.base)
    showSave('полив запущен', 'ok')
  } catch (e) {
    showSave(e instanceof Error ? e.message : 'ошибка запуска полива', 'err')
  } finally {
    busy.value = false
  }
}

async function onCheckUpdate() {
  otaBusy.value = true
  try {
    updateStatus.value = await checkUpdate(props.base)
  } catch (e) {
    updateStatus.value = {
      ...updateStatus.value,
      state: 'error',
      error: e instanceof Error ? e.message : 'ошибка проверки',
    }
  } finally {
    otaBusy.value = false
  }
}

async function onStartUpdate() {
  otaBusy.value = true
  try {
    await startUpdate(props.base)
    showSave('обновление скачивается, устройство скоро перезагрузится', 'ok')
  } catch (e) {
    updateStatus.value = {
      ...updateStatus.value,
      state: 'error',
      error: e instanceof Error ? e.message : 'ошибка запуска',
    }
  } finally {
    otaBusy.value = false
  }
}

let statusInterval: ReturnType<typeof setInterval>
let otaInterval: ReturnType<typeof setInterval>

onMounted(async () => {
  await load()
  await updateStatusInfo()
  await updateOtaInfo()

  statusInterval = setInterval(updateStatusInfo, 5000)
  otaInterval = setInterval(updateOtaInfo, 5000)

  watch(
    () => settings.value.mode,
    () => updateStatusInfo()
  )
})

onUnmounted(() => {
  clearInterval(statusInterval)
  clearInterval(otaInterval)
})
</script>

<style scoped>
.device-head {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 0.5rem;
  margin-bottom: 1rem;
  flex-wrap: wrap;
}
.device-name {
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--text);
}
.self-badge {
  font-size: 0.75rem;
  font-weight: 500;
  color: var(--text-muted);
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  padding: 0.1rem 0.4rem;
  margin-left: 0.4rem;
  vertical-align: middle;
}
.device-meta {
  font-size: 0.85rem;
  color: var(--text-muted);
}
.device-status {
  color: var(--text-muted);
  display: flex;
  align-items: center;
  gap: 0.75rem;
}
.ota-details {
  margin-top: 1rem;
  border-top: 1px solid var(--border);
  padding-top: 0.75rem;
}
.ota-details summary {
  cursor: pointer;
  font-weight: 500;
  color: var(--text-muted);
  margin-bottom: 0.75rem;
}
</style>
