import type { Manifest, ManifestImage, ManifestProject } from "./types";

const MANIFEST_FILE = "firmware/manifest.json";

/** Vite base 경로를 반영한 assets URL */
export function assetUrl(relativePath: string): string {
  return `${import.meta.env.BASE_URL}${relativePath.replace(/^\/+/, "")}`;
}

export function projectSize(project: ManifestProject): number {
  return project.images.reduce((sum, image) => sum + image.size, 0);
}

export async function loadManifest(): Promise<Manifest> {
  const response = await fetch(assetUrl(MANIFEST_FILE), { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`펌웨어 매니페스트를 불러오지 못했습니다. (HTTP ${response.status})`);
  }
  const manifest = (await response.json()) as Manifest;
  validateManifest(manifest);
  return manifest;
}

function validateManifest(manifest: Manifest): void {
  if (!manifest || manifest.schemaVersion !== 1 || !Array.isArray(manifest.projects)) {
    throw new Error("지원하지 않는 펌웨어 매니페스트 형식입니다.");
  }
  for (const project of manifest.projects) {
    if (!project.id || !project.name || !Array.isArray(project.images) || project.images.length === 0) {
      throw new Error(`프로젝트 매니페스트가 올바르지 않습니다: ${project.id ?? "(unknown)"}`);
    }
    for (const image of project.images) {
      if (!/^0x[0-9a-fA-F]+$/.test(image.offset) || !image.path || !image.sha256 || !image.size) {
        throw new Error(`이미지 정보가 올바르지 않습니다: ${project.id} / ${image.path ?? image.offset}`);
      }
    }
  }
}

/** 이미지 다운로드 + SHA-256 무결성 검증 */
export async function fetchImage(image: ManifestImage): Promise<Uint8Array> {
  const response = await fetch(`${assetUrl(image.path)}?v=${image.sha256.slice(0, 12)}`);
  if (!response.ok) {
    throw new Error(`펌웨어 파일 다운로드 실패: ${image.path} (HTTP ${response.status})`);
  }
  const data = new Uint8Array(await response.arrayBuffer());
  if (data.length !== image.size) {
    throw new Error(`펌웨어 파일 크기가 매니페스트와 다릅니다: ${image.path}`);
  }
  const digest = await sha256Hex(data);
  if (digest !== image.sha256) {
    throw new Error(`펌웨어 무결성 검증에 실패했습니다: ${image.path}\n페이지를 새로고침한 뒤 다시 시도해 주세요.`);
  }
  return data;
}

async function sha256Hex(data: Uint8Array): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", data.buffer as ArrayBuffer);
  return [...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, "0")).join("");
}
