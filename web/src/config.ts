import type { ClientEntry, ClientPlatform } from "./types";

/** DeskStream 저장소 정보 (README·클라이언트 다운로드 링크 생성에 사용) */
export const REPOSITORY_URL = "https://github.com/worynim/DeskStream";
export const REPOSITORY_BRANCH = "main";

/** 웹 플래셔 버전 (web/package.json의 version을 빌드 시 주입) */
export const WEB_VERSION: string = __APP_VERSION__;

/** 웹 플래셔 릴리즈 노트 (GitHub) */
export const RELEASE_NOTES_URL = `${REPOSITORY_URL}/blob/${REPOSITORY_BRANCH}/web/RELEASE_NOTES.md`;

/**
 * PC 클라이언트 실행 파일 다운로드 링크 (GitHub Release)
 * 태그·파일명 규칙은 .github/workflows/build-clients.yml 과 같다.
 */
export function clientDownloadUrl(client: ClientEntry, platform: ClientPlatform): string {
  const tag = `${client.project}-client-v${client.version}`;
  const ext = platform.startsWith("macos") ? "zip" : "exe";
  const file = `${client.app}-${client.version}-${platform}.${ext}`;
  return `${REPOSITORY_URL}/releases/download/${encodeURIComponent(tag)}/${encodeURIComponent(file)}`;
}

/** 프로젝트별 README 링크 (GitHub) */
export function readmeUrl(projectId: string): string {
  return `${REPOSITORY_URL}/blob/${REPOSITORY_BRANCH}/${encodeURIComponent(projectId)}/README.md`;
}
