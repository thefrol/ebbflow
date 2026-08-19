<template>
  <div>
    <header>
      <h1>ebbflow — флот</h1>
      <div class="meta">
        <span>устройств: {{ 1 + peers.length }}</span>
        <span v-if="peersLoading && peers.length === 0">поиск соседей…</span>
      </div>
    </header>

    <DeviceCard base="" :is-self="true" />

    <DeviceCard v-for="peer in peers" :key="peer.id" :base="peerBase(peer)" />

    <div v-if="!peersLoading && peers.length === 0" class="empty">
      соседних устройств не найдено
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import DeviceCard from './components/DeviceCard.vue'
import { getPeers } from './api.ts'
import type { LampPeer } from './types.ts'

const peers = ref<LampPeer[]>([])
const peersLoading = ref(true)

function peerBase(peer: LampPeer) {
  // Надёжнее идти по IP: hostname соседа может не резолвиться с клиента.
  return `http://${peer.ip}:${peer.port}`
}

async function updatePeers() {
  try {
    peers.value = await getPeers()
  } catch (e) {
    // не трогаем текущий список — временная ошибка сети не должна
    // сбрасывать открытые карточки
  } finally {
    peersLoading.value = false
  }
}

onMounted(() => {
  // Поиск соседей по mDNS не блокирует отрисовку своей карточки.
  updatePeers()
  setInterval(updatePeers, 30000)
})
</script>
