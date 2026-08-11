import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import zlib from 'node:zlib'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const distDir = path.resolve(__dirname, '../dist')
const indexPath = path.join(distDir, 'index.html')
const gzPath = path.join(distDir, 'index.html.gz')

const html = fs.readFileSync(indexPath)
const gz = zlib.gzipSync(html, { level: 9 })
fs.writeFileSync(gzPath, gz)

const raw = html.length
const compressed = gz.length
console.log(`gzipped index.html: ${raw} -> ${compressed} bytes (${Math.round((compressed / raw) * 100)}%)`)
