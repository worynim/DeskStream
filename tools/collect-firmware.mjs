#!/usr/bin/env node
/**
 * DeskStream 웹 플래셔용 펌웨어 수집 스크립트
 *
 * Arduino IDE 의 "Export Compiled Binary" 결과(build/**\/flash_args)에서
 * 실제 업로드에 필요한 .bin 4종만 firmware/<project>/ 로 복사하고,
 * 웹 플래셔가 읽는 firmware/manifest.json 을 생성합니다.
 *
 * 사용법:
 *   node tools/collect-firmware.mjs                  # 전체 수집 + manifest 생성
 *   node tools/collect-firmware.mjs --project Hangeul_Clock
 *   node tools/collect-firmware.mjs --check          # CI: 커밋된 manifest/바이너리·clients.json 검증
 */
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const META_PATH = path.join(ROOT, "firmware", "projects.json");
const MANIFEST_PATH = path.join(ROOT, "firmware", "manifest.json");
const CLIENTS_PATH = path.join(ROOT, "firmware", "clients.json");
const CLIENT_PLATFORMS = new Set(["macos-arm64", "windows-x64"]);

const argv = process.argv.slice(2);
const CHECK = argv.includes("--check");
const projectIdx = argv.indexOf("--project");
const PROJECT_FILTER = projectIdx >= 0 ? argv[projectIdx + 1] : null;

const SOURCE_EXT = new Set([".ino", ".c", ".cpp", ".cxx", ".h", ".hpp", ".hh"]);
// 공통 라이브러리 — 이를 include 하는 스케치는 라이브러리 변경도 "소스 최신" 판정에 포함한다
const CORE_LIB_DIR = path.join(ROOT, "libraries", "DeskStreamCore");
const CORE_LIB_HEADER = "DeskStreamCore.h";
const FLASH_MODE = new Set(["qio", "qout", "dio", "dout"]);
const FLASH_FREQ = new Set(["80m", "40m", "26m", "20m"]);

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, "utf8"));
}

function sha256(buf) {
  return crypto.createHash("sha256").update(buf).digest("hex");
}

function toPosix(p) {
  return p.split(path.sep).join("/");
}

/** 스케치 폴더 최상위 소스 중 하나라도 공통 라이브러리 헤더를 include 하는가 */
function usesCoreLib(sketchDir) {
  let entries = [];
  try {
    entries = fs.readdirSync(sketchDir, { withFileTypes: true });
  } catch {
    return false;
  }
  return entries.some((entry) => {
    if (!entry.isFile() || !SOURCE_EXT.has(path.extname(entry.name).toLowerCase())) return false;
    try {
      return fs.readFileSync(path.join(sketchDir, entry.name), "utf8").includes(CORE_LIB_HEADER);
    } catch {
      return false;
    }
  });
}

function newestMtime(dir, predicate, skipDirNames) {
  let newest = 0;
  const stack = [dir];
  while (stack.length) {
    const current = stack.pop();
    let entries;
    try {
      entries = fs.readdirSync(current, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) {
        if (skipDirNames.has(entry.name)) continue;
        stack.push(full);
      } else if (entry.isFile() && predicate(entry.name)) {
        try {
          const m = fs.statSync(full).mtimeMs;
          if (m > newest) newest = m;
        } catch {
          /* ignore */
        }
      }
    }
  }
  return newest;
}

/** build/ 및 build/<fqbn>/ 에서 flash_args 후보를 찾는다. */
function findExportSets(project) {
  const sketchDir = path.join(ROOT, project.sourcePath);
  const buildDir = path.join(sketchDir, "build");
  const sets = [];
  if (fs.existsSync(path.join(buildDir, "flash_args"))) sets.push(buildDir);
  let entries = [];
  try {
    entries = fs.readdirSync(buildDir, { withFileTypes: true });
  } catch {
    return sets;
  }
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const candidate = path.join(buildDir, entry.name);
    if (fs.existsSync(path.join(candidate, "flash_args"))) sets.push(candidate);
  }
  return sets;
}

/** flash_args 파싱: { mode, freq, size, images: [{ offset, file }] } */
function parseFlashArgs(file) {
  const lines = fs
    .readFileSync(file, "utf8")
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);
  const header = lines.shift() ?? "";
  const mode = header.match(/--flash-mode\s+(\S+)/)?.[1]?.toLowerCase();
  const freq = header.match(/--flash-freq\s+(\S+)/)?.[1]?.toLowerCase();
  const size = header.match(/--flash-size\s+(\S+)/)?.[1]?.toUpperCase();
  const images = [];
  for (const line of lines) {
    const match = line.match(/^(0x[0-9a-fA-F]+)\s+(.+)$/);
    if (!match) continue;
    images.push({ offset: match[1].toLowerCase(), file: match[2].trim() });
  }
  return { mode, freq, size, images };
}

function readFqbn(dir) {
  const optionsPath = path.join(dir, "build.options.json");
  try {
    return readJson(optionsPath).fqbn ?? null;
  } catch {
    return null;
  }
}

function partitionSchemeOf(fqbn) {
  return fqbn?.match(/PartitionScheme=([^,]+)/)?.[1] ?? null;
}

/** partitions.csv 에서 app0 파티션 크기(바이트)를 찾는다. */
function app0SizeOf(dir) {
  const csvPath = path.join(dir, "partitions.csv");
  try {
    for (const line of fs.readFileSync(csvPath, "utf8").split(/\r?\n/)) {
      const cols = line.split(",").map((c) => c.trim());
      if (cols[0] === "app0" && cols[4]) {
        const size = Number.parseInt(cols[4], 16);
        if (Number.isFinite(size)) return size;
      }
    }
  } catch {
    /* ignore */
  }
  return null;
}

function completeSet(setDir, parsed) {
  if (!parsed.mode || !parsed.freq || !parsed.size || parsed.images.length === 0) return false;
  if (!FLASH_MODE.has(parsed.mode) || !FLASH_FREQ.has(parsed.freq)) return false;
  return parsed.images.every((image) => fs.existsSync(path.join(setDir, image.file)));
}

function collectProject(project) {
  const sets = findExportSets(project)
    .map((dir) => {
      const flashArgsPath = path.join(dir, "flash_args");
      let mtime = 0;
      try {
        mtime = fs.statSync(flashArgsPath).mtimeMs;
      } catch {
        /* ignore */
      }
      return { dir, mtime, parsed: parseFlashArgs(flashArgsPath) };
    })
    .filter((set) => completeSet(set.dir, set.parsed))
    .sort((a, b) => b.mtime - a.mtime);

  if (sets.length === 0) {
    return { ok: false, reason: "완전한 export(build/**/flash_args)를 찾지 못했습니다." };
  }

  const set = sets[0];
  const { mode, freq, size, images } = set.parsed;
  const appLimit = app0SizeOf(set.dir);
  const seenOffsets = new Set();
  const outputImages = [];

  for (const image of images) {
    if (seenOffsets.has(image.offset)) {
      return { ok: false, reason: `중복 오프셋 ${image.offset}` };
    }
    seenOffsets.add(image.offset);

    const src = path.join(set.dir, image.file);
    const base = path.basename(image.file);
    const destDir = path.join(ROOT, "firmware", project.id);
    fs.mkdirSync(destDir, { recursive: true });
    const dest = path.join(destDir, base);
    fs.copyFileSync(src, dest);
    const buf = fs.readFileSync(dest);

    if (image.offset === "0x10000" && appLimit && buf.length > appLimit) {
      return {
        ok: false,
        reason: `앱 이미지(${buf.length}B)가 app0 파티션(${appLimit}B)을 초과합니다.`,
      };
    }
    if (Number.parseInt(image.offset, 16) + buf.length > 4 * 1024 * 1024) {
      return { ok: false, reason: `${base} 이미지가 4MB 플래시 범위를 벗어납니다.` };
    }

    outputImages.push({
      offset: image.offset,
      path: toPosix(path.relative(ROOT, dest)),
      size: buf.length,
      sha256: sha256(buf),
    });
  }

  outputImages.sort((a, b) => Number.parseInt(a.offset, 16) - Number.parseInt(b.offset, 16));

  const fqbn = readFqbn(set.dir);
  const sketchDir = path.join(ROOT, project.sourcePath);
  const isSource = (name) => SOURCE_EXT.has(path.extname(name).toLowerCase());
  const skipDirs = new Set(["build", ".git", "node_modules"]);
  let sourceNewest = newestMtime(sketchDir, isSource, skipDirs);
  if (usesCoreLib(sketchDir)) {
    // 라이브러리의 호스트 테스트(test/)는 펌웨어에 들어가지 않으므로 src/ 만 본다
    sourceNewest = Math.max(sourceNewest, newestMtime(path.join(CORE_LIB_DIR, "src"), isSource, skipDirs));
  }
  const stale = sourceNewest > set.mtime + 1000;

  return {
    ok: true,
    stale,
    entry: {
      id: project.id,
      name: project.name,
      version: project.version,
      icon: project.icon ?? "",
      order: project.order ?? 999,
      description: project.description ?? "",
      note: project.note ?? "",
      nameEn: project.nameEn ?? "",
      descriptionEn: project.descriptionEn ?? "",
      noteEn: project.noteEn ?? "",
      fqbn,
      partitionScheme: partitionSchemeOf(fqbn),
      flash: { mode, freq, size },
      exportedAt: new Date(set.mtime).toISOString(),
      sourcePath: project.sourcePath,
      sourceCommit: gitHead(),
      images: outputImages,
    },
  };
}

function gitHead() {
  try {
    const head = fs.readFileSync(path.join(ROOT, ".git", "HEAD"), "utf8").trim();
    if (head.startsWith("ref: ")) {
      return fs.readFileSync(path.join(ROOT, ".git", head.slice(5)), "utf8").trim();
    }
    return head;
  } catch {
    return null;
  }
}

function pad(value, width) {
  const s = String(value);
  return s + " ".repeat(Math.max(0, width - s.length));
}

function formatBytes(bytes) {
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
}

function validateCommittedManifest(meta) {
  const errors = [];
  if (!fs.existsSync(MANIFEST_PATH)) {
    return [`manifest.json 이 없습니다: ${path.relative(ROOT, MANIFEST_PATH)}`];
  }
  let manifest;
  try {
    manifest = readJson(MANIFEST_PATH);
  } catch (error) {
    return [`manifest.json 파싱 실패: ${error.message}`];
  }
  if (manifest.schemaVersion !== 1) errors.push(`지원하지 않는 schemaVersion: ${manifest.schemaVersion}`);
  if (!Array.isArray(manifest.projects)) return [...errors, "projects 배열이 없습니다."];

  const metaIds = new Set(meta.projects.map((p) => p.id));
  const ids = new Set();
  for (const project of manifest.projects) {
    if (!metaIds.has(project.id)) errors.push(`projects.json 에 없는 프로젝트: ${project.id}`);
    if (ids.has(project.id)) errors.push(`중복 프로젝트 id: ${project.id}`);
    ids.add(project.id);

    if (!Array.isArray(project.images) || project.images.length === 0) {
      errors.push(`${project.id}: images 가 비어 있습니다.`);
      continue;
    }
    const offsets = new Set();
    for (const image of project.images) {
      if (offsets.has(image.offset)) errors.push(`${project.id}: 중복 오프셋 ${image.offset}`);
      offsets.add(image.offset);
      const abs = path.join(ROOT, image.path);
      if (!fs.existsSync(abs)) {
        errors.push(`${project.id}: 파일 없음 ${image.path}`);
        continue;
      }
      const buf = fs.readFileSync(abs);
      if (buf.length !== image.size) {
        errors.push(`${project.id}: 크기 불일치 ${image.path} (${buf.length} != ${image.size})`);
      }
      if (sha256(buf) !== image.sha256) {
        errors.push(`${project.id}: sha256 불일치 ${image.path}`);
      }
    }
  }
  for (const id of metaIds) {
    if (!ids.has(id)) errors.push(`manifest 에 누락된 프로젝트: ${id} (export 여부 확인)`);
  }
  return errors;
}

/**
 * firmware/clients.json(PC 클라이언트 다운로드 목록) 검증
 * - 프로젝트가 projects.json 에 있고, 플랫폼 이름이 정해진 값이며,
 * - 프로젝트 폴더의 PyInstaller spec 의 APP_NAME / VERSION 과 일치해야 한다 (Release 파일명과 태그가 여기서 정해짐).
 */
function validateClients(meta) {
  if (!fs.existsSync(CLIENTS_PATH)) return [];
  let data;
  try {
    data = readJson(CLIENTS_PATH);
  } catch (error) {
    return [`clients.json 파싱 실패: ${error.message}`];
  }
  if (data.schemaVersion !== 1 || !Array.isArray(data.clients)) {
    return ["clients.json 형식이 올바르지 않습니다 (schemaVersion 1, clients 배열)."];
  }

  const errors = [];
  const metaIds = new Set(meta.projects.map((p) => p.id));
  for (const client of data.clients) {
    const id = client.project;
    if (!metaIds.has(id)) {
      errors.push(`clients.json: projects.json 에 없는 프로젝트 ${id}`);
      continue;
    }
    if (!Array.isArray(client.platforms) || client.platforms.length === 0) {
      errors.push(`clients.json: ${id} platforms 가 비어 있습니다.`);
    } else {
      for (const platform of client.platforms) {
        if (!CLIENT_PLATFORMS.has(platform)) errors.push(`clients.json: ${id} 알 수 없는 플랫폼 ${platform}`);
      }
    }
    const specs = findSpecs(path.join(ROOT, id));
    if (specs.length !== 1) {
      errors.push(`clients.json: ${id} 폴더에 .spec 파일이 정확히 1개여야 합니다 (발견 ${specs.length})`);
      continue;
    }
    const spec = fs.readFileSync(specs[0], "utf8");
    const app = spec.match(/^APP_NAME = "([^"]+)"/m)?.[1];
    const version = spec.match(/^VERSION = "([^"]+)"/m)?.[1];
    if (app !== client.app) errors.push(`clients.json: ${id} app(${client.app}) ≠ spec APP_NAME(${app})`);
    if (version !== client.version) {
      errors.push(`clients.json: ${id} version(${client.version}) ≠ spec VERSION(${version})`);
    }
  }
  return errors;
}

function findSpecs(dir) {
  const found = [];
  const stack = [dir];
  while (stack.length) {
    const current = stack.pop();
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) {
        if (!["build", "dist", ".git", "node_modules", "__pycache__"].includes(entry.name)) stack.push(full);
      } else if (entry.name.endsWith(".spec")) {
        found.push(full);
      }
    }
  }
  return found;
}

function main() {
  const meta = readJson(META_PATH);

  if (CHECK) {
    const errors = [...validateCommittedManifest(meta), ...validateClients(meta)];
    if (errors.length) {
      console.error("❌ firmware manifest 검증 실패");
      for (const error of errors) console.error(`  - ${error}`);
      process.exit(1);
    }
    console.log(`✅ firmware manifest 검증 통과 (${meta.projects.length}개 프로젝트)`);
    return;
  }

  const projects = PROJECT_FILTER
    ? meta.projects.filter((project) => project.id === PROJECT_FILTER)
    : meta.projects;
  if (projects.length === 0) {
    console.error(`❌ 알 수 없는 프로젝트: ${PROJECT_FILTER}`);
    process.exit(1);
  }

  const collected = [];
  const skipped = [];
  const stale = [];

  console.log("프로젝트별 export 수집");
  console.log("─".repeat(96));
  for (const project of projects) {
    const result = collectProject(project);
    if (!result.ok) {
      skipped.push({ project, reason: result.reason });
      console.log(`${pad(project.id, 30)} ${pad("SKIP", 6)} ${result.reason}`);
      continue;
    }
    collected.push(result.entry);
    if (result.stale) stale.push(result.entry.id);
    console.log(
      `${pad(project.id, 30)} ${pad("OK", 6)} ${pad(formatBytes(result.entry.images.reduce((sum, i) => sum + i.size, 0)), 10)} ${result.entry.flash.mode}/${result.entry.flash.freq}/${result.entry.flash.size}${result.stale ? "  ⚠️ 소스가 바이너리보다 최신" : ""}`,
    );
  }

  collected.sort((a, b) => a.order - b.order);
  const manifest = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    chip: meta.chip,
    board: meta.board,
    flashDefaults: meta.flashDefaults,
    projects: collected,
  };
  fs.writeFileSync(MANIFEST_PATH, `${JSON.stringify(manifest, null, 2)}\n`);

  // 제외된 프로젝트의 이전 산출물 정리
  for (const { project } of skipped) {
    if (!PROJECT_FILTER) fs.rmSync(path.join(ROOT, "firmware", project.id), { recursive: true, force: true });
  }

  console.log("─".repeat(96));
  console.log(`✅ ${collected.length}개 프로젝트 → ${toPosix(path.relative(ROOT, MANIFEST_PATH))}`);
  if (skipped.length) console.log(`ℹ️  ${skipped.length}개 프로젝트는 export가 없어 제외되었습니다.`);
  if (stale.length) {
    console.log(`⚠️  소스가 바이너리보다 최신입니다. Arduino IDE에서 다시 export 하세요: ${stale.join(", ")}`);
  }
}

main();
