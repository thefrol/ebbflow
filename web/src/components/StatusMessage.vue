<template>
  <div v-if="text" class="status" :class="kind">
    {{ text }}
  </div>
</template>

<script setup lang="ts">
defineProps<{
  text: string
  kind?: 'ok' | 'err' | 'info' | ''
}>()
</script>

<style scoped>
.status {
  padding: 0.75rem;
  border-radius: 0.5rem;
  font-size: 0.95rem;
  margin-top: 0.75rem;
  min-height: 1.2em;
}
.status.ok {
  color: var(--ok-text);
  background: var(--ok-bg);
}
.status.err {
  color: var(--err-text);
  background: var(--err-bg);
}
.status.info {
  color: var(--info-text);
  background: var(--info-bg);
}
</style>
