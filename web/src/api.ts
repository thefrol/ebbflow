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

// base — префикс устройства: '' для той лампы, что отдала страницу,
// 'http://<ip>' для соседей (на прошивке включён CORS).
export function getInfo(base = '') {
  return req<LampInfo>(base + '/api/info')
}

export function getPeers(base = '') {
  return req<LampPeer[]>(base + '/api/peers')
}

export function getSettings(base = '') {
  return req<LampSettings>(base + '/api/settings')
}

export function saveSettings(body: LampSettings, base = '') {
  return req<LampSettings>(base + '/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
}

export function getStatus(base = '') {
  return req<LampStatus>(base + '/api/status')
}

export function waterNow(base = '') {
  return req<LampStatus>(base + '/api/water-now', { method: 'POST' })
}

export function checkUpdate(base = '') {
  return req<UpdateStatus>(base + '/api/update/check', { method: 'POST' })
}

export function startUpdate(base = '') {
  return req<{ status: string }>(base + '/api/update/start', { method: 'POST' })
}

export function getUpdateStatus(base = '') {
  return req<UpdateStatus>(base + '/api/update/status')
}
