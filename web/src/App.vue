<template>
  <div>
    <header>
      <h1>ebbflow-lamp</h1>
      <div class="meta">
        <span v-if="info">{{ info.name }}</span>
        <span v-if="info">v{{ info.version }}</span>
        <span v-if="info">{{ info.chip }}</span>
      </div>
    </header>

    <div v-if="loading" class="loading">загрузка…</div>

    <template v-else>
      <Card title="Настройки">
        <form @submit.prevent="onSubmit">
          <div class="form-group">
            <label for="name">Имя устройства</label>
            <input id="name" v-model="settings.name" type="text" maxlength="31" required />
          </div>

          <div class="form-group">
            <label for="mode">Режим</label>
            <select id="mode" v-model="settings.mode">
              <option value="schedule">Расписание (свет)</option>
              <option value="pulse">Импульсы (полив)</option>
            </select>
          </div>

          <ScheduleForm v-if="settings.mode === 'schedule'" v-model="settings" />
          <PulseForm
            v-else
            v-model="settings"
            :status="status"
            :busy="busy"
            @water="onWater"
          />

          <div class="form-group">
            <label for="gpio">GPIO лампы</label>
            <input id="gpio" v-model.number="settings.gpio" type="number" min="0" max="21" required />
          </div>

          <div class="form-group row">
            <input id="enabled" v-model="settings.enabled" type="checkbox" />
            <label for="enabled">Расписание активно</label>
          </div>

          <button type="submit" :disabled="saving">
            {{ saving ? 'Сохраняю…' : 'Сохранить' }}
          </button>
          <StatusMessage :text="saveMessage" :kind="saveKind" />
        </form>
      </Card>

      <Card title="Другие лампы">
        <div v-if="peersLoading" class="peers-status">поиск…</div>
        <div v-else-if="peers.length === 0" class="peers-status">никого не найдено</div>
        <ul v-else class="peers-list">
          <li v-for="peer in peers" :key="peer.id">
            <a :href="peerUrl(peer)" target="_blank" rel="noopener">
              {{ peer.name }}
            </a>
            <span class="peer-meta">· v{{ peer.version }} · {{ peer.chip }} · {{ peer.mode }}</span>
          </li>
        </ul>
      </Card>

      <Card title="Обновление прошивки">
        <OtaPanel :status="updateStatus" :busy="otaBusy" @check="onCheckUpdate" @start="onStartUpdate" />
      </Card>
    </template>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import Card from './components/Card.vue'
import StatusMessage from './components/StatusMessage.vue'
import ScheduleForm from './components/ScheduleForm.vue'
import PulseForm from './components/PulseForm.vue'
import OtaPanel from './components/OtaPanel.vue'
import {
  getInfo,
  getPeers,
  getSettings,
  saveSettings,
  getStatus,
  getUpdateStatus,
  waterNow,
  checkUpdate,
  startUpdate,
} from './api.ts'
import type { LampInfo, LampPeer, LampSettings, LampStatus, UpdateStatus } from './types.ts'

const loading = ref(true)
const saving = ref(false)
const busy = ref(false)
const otaBusy = ref(false)
const saveMessage = ref('')
const saveKind = ref<'ok' | 'err' | 'info' | ''>('')
const peersLoading = ref(false)

const info = ref<LampInfo | null>(null)
const settings = ref<LampSettings>({
  mode: 'schedule',
  name: 'ebbflow-lamp',
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
const peers = ref<LampPeer[]>([])

async function load() {
  try {
    const [i, s] = await Promise.all([getInfo(), getSettings()])
    info.value = i
    settings.value = s
  } catch (e) {
    showSave('не удалось загрузить настройки', 'err')
  }
}

async function updatePeers() {
  peersLoading.value = true
  try {
    peers.value = await getPeers()
  } catch (e) {
    peers.value = []
  } finally {
    peersLoading.value = false
  }
}

function peerUrl(peer: LampPeer) {
  if (peer.host) {
    return `http://${peer.host}/`
  }
  return `http://${peer.ip}:${peer.port}/`
}

async function updateStatusInfo() {
  try {
    status.value = await getStatus()
  } catch (e) {
    // молча игнорируем — ошибка видна в основной загрузке
  }
}

async function updateOtaInfo() {
  try {
    updateStatus.value = await getUpdateStatus()
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
    await saveSettings(settings.value)
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
    status.value = await waterNow()
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
    updateStatus.value = await checkUpdate()
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
    await startUpdate()
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

onMounted(async () => {
  await load()
  await updateStatusInfo()
  await updateOtaInfo()
  await updatePeers()
  loading.value = false

  const statusInterval = setInterval(updateStatusInfo, 5000)
  const otaInterval = setInterval(updateOtaInfo, 5000)
  const peersInterval = setInterval(updatePeers, 30000)

  watch(
    () => settings.value.mode,
    () => updateStatusInfo()
  )
})
</script>
