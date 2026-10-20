import { readFileSync } from "node:fs";
import { defineConfig } from "vite";

// GitHub Pages 프로젝트 사이트 기본 경로. 커스텀 도메인으로 옮길 때 BASE_PATH=/ 로 지정.
const base = process.env.BASE_PATH || "/DeskStream/";

// package.json 버전을 빌드 결과에 주입해 릴리즈 노트 링크에 표시한다.
const pkg = JSON.parse(readFileSync(new URL("./package.json", import.meta.url), "utf-8")) as {
  version: string;
};

export default defineConfig({
  base,
  define: {
    __APP_VERSION__: JSON.stringify(pkg.version),
  },
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
