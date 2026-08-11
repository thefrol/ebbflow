import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [vue()],
  build: {
    // Отключаем хэши в именах файлов, чтобы post-build скрипт мог найти ассеты
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name].js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name].[ext]',
      },
    },
    cssCodeSplit: false,
    // Не нужен sourcemap в прошивке
    sourcemap: false,
    // Минификация по умолчанию включена
    minify: true,
  },
})
