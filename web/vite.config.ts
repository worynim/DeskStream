import { defineConfig } from "vite";

// GitHub Pages 프로젝트 사이트 기본 경로. 커스텀 도메인으로 옮길 때 BASE_PATH=/ 로 지정.
const base = process.env.BASE_PATH || "/DeskStream/";

export default defineConfig({
  base,
  build: {
    target: "es2022",
    outDir: "dist",
    assetsDir: "assets",
    sourcemap: false,
  },
  server: {
    port: 5173,
    strictPort: false,
  },
  preview: {
    port: 4173,
  },
});
