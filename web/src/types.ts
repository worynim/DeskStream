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
