export interface LampSettings {
  mode: 'schedule' | 'pulse'
  name: string
  on: string
  off: string
  pulse_interval_min: number
  pulse_duration_sec: number
  gpio: number
  enabled: boolean
}

export interface LampInfo {
  name: string
  version: string
  chip: string
}

export interface LampPeer {
  id: string
  name: string
  host: string
  ip: string
  port: number
  version: string
  chip: string
  mode: 'schedule' | 'pulse'
}

export interface PulseTime {
  time: string
  day_offset: number
  past: boolean
}

export interface LampStatus {
  mode: 'schedule' | 'pulse'
  enabled: boolean
  manual_pulse_active: boolean
  next_pulse_time: string | null
  next_pulse_today: boolean
  pulse_times: PulseTime[]
}

export type OtaState =
  | 'idle'
  | 'checking'
  | 'update_available'
  | 'downloading'
  | 'reboot_pending'
  | 'up_to_date'
  | 'error'

export interface UpdateStatus {
  current_version: string
  available_version: string
  state: OtaState
  error: string
}
