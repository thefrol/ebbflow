import type { LampInfo, LampPeer, LampSettings, LampStatus, UpdateStatus } from './types.ts'

class ApiError extends Error {
  constructor(message: string) {
    super(message)
    this.name = 'ApiError'
  }
}

async function req<T>(url: string, init?: RequestInit): Promise<T> {
  const r = await fetch(url, init)
  if (!r.ok) {
    const text = await r.text().catch(() => 'unknown error')
    throw new ApiError(text)
  }
  return r.json() as Promise<T>
}

export function getInfo() {
  return req<LampInfo>('/api/info')
}

export function getPeers() {
  return req<LampPeer[]>('/api/peers')
}

export function getSettings() {
  return req<LampSettings>('/api/settings')
}

export function saveSettings(body: LampSettings) {
  return req<LampSettings>('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
}

export function getStatus() {
  return req<LampStatus>('/api/status')
}

export function waterNow() {
  return req<LampStatus>('/api/water-now', { method: 'POST' })
}

export function checkUpdate() {
  return req<UpdateStatus>('/api/update/check', { method: 'POST' })
}

export function startUpdate() {
  return req<{ status: string }>('/api/update/start', { method: 'POST' })
}

export function getUpdateStatus() {
  return req<UpdateStatus>('/api/update/status')
}
