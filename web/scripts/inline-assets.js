import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const distDir = path.resolve(__dirname, '../dist')
const indexPath = path.join(distDir, 'index.html')

let html = fs.readFileSync(indexPath, 'utf-8')

// Inline CSS
const cssLinkMatch = html.match(/<link[^>]*rel="stylesheet"[^>]*href="\/assets\/([^"]+)"[^>]*>/)
if (cssLinkMatch) {
  const cssName = cssLinkMatch[1]
  const cssPath = path.join(distDir, 'assets', cssName)
  const css = fs.readFileSync(cssPath, 'utf-8')
  html = html.replace(cssLinkMatch[0], () => `<style>${css}</style>`)
  fs.rmSync(cssPath, { force: true })
}

// Inline JS
const scriptMatch = html.match(/<script[^>]*type="module"[^>]*src="\/assets\/([^"]+)"[^>]*><\/script>/)
if (scriptMatch) {
  const jsName = scriptMatch[1]
  const jsPath = path.join(distDir, 'assets', jsName)
  const js = fs.readFileSync(jsPath, 'utf-8')
    // Экранируем </script> внутри inline JS, иначе браузер преждевременно
    // закроет <script> и остаток страницы отобразится как текст.
    .replace(/<\/script>/gi, '<\\/script>')
  html = html.replace(scriptMatch[0], () => `<script type="module">${js}</script>`)
  fs.rmSync(jsPath, { force: true })
}

// Удаляем пустую папку assets, если осталась
const assetsDir = path.join(distDir, 'assets')
if (fs.existsSync(assetsDir) && fs.readdirSync(assetsDir).length === 0) {
  fs.rmdirSync(assetsDir)
}

fs.writeFileSync(indexPath, html)
console.log('inlined assets into index.html')
