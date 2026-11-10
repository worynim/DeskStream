export interface ManifestImage {
  offset: string;
  path: string;
  size: number;
  sha256: string;
}

export interface ManifestProject {
  id: string;
  name: string;
  nameEn?: string;
  version: string;
  icon: string;
  order: number;
  description: string;
  descriptionEn?: string;
  note: string;
  noteEn?: string;
  fqbn: string | null;
  partitionScheme: string | null;
  flash: { mode: string; freq: string; size: string };
  exportedAt: string;
  sourcePath: string;
  sourceCommit: string | null;
  images: ManifestImage[];
}

export interface Manifest {
  schemaVersion: number;
  generatedAt: string;
  chip: string;
  board: string;
  flashDefaults: { mode: string; freq: string; size: string };
  projects: ManifestProject[];
}

/** PC 클라이언트 실행 파일이 제공되는 플랫폼 (Release 파일명 접미사) */
export type ClientPlatform = "macos-arm64" | "windows-x64";

/** firmware/clients.json 의 항목 — 프로젝트에 딸린 PC 클라이언트 */
export interface ClientEntry {
  project: string;
  app: string;
  version: string;
  platforms: ClientPlatform[];
}

export interface ClientList {
  schemaVersion: number;
  clients: ClientEntry[];
}
