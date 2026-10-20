#!/usr/bin/env node
// firmware/ (저장소에서 추적되는 펌웨어) → web/public/firmware/ 로 동기화.
// dev/build 전에 자동 실행되어 GitHub Pages 산출물에 바이너리가 포함되게 한다.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const WEB_DIR = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const SRC = path.resolve(WEB_DIR, "..", "firmware");
const DEST = path.resolve(WEB_DIR, "public", "firmware");

if (!fs.existsSync(path.join(SRC, "manifest.json"))) {
  console.warn("[sync-firmware] firmware/manifest.json 이 없습니다. 먼저 실행하세요: node tools/collect-firmware.mjs");
  process.exit(1);
}

fs.rmSync(DEST, { recursive: true, force: true });
fs.mkdirSync(path.dirname(DEST), { recursive: true });
fs.cpSync(SRC, DEST, { recursive: true });

const projects = JSON.parse(fs.readFileSync(path.join(SRC, "manifest.json"), "utf8")).projects ?? [];
console.log(`[sync-firmware] firmware/ → web/public/firmware/ (${projects.length}개 프로젝트)`);
