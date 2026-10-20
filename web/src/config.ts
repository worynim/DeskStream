/** DeskStream 저장소 정보 (README 링크 생성에 사용) */
export const REPOSITORY_URL = "https://github.com/worynim/DeskStream";
export const REPOSITORY_BRANCH = "main";

/** 웹 플래셔 버전 (릴리즈 노트와 함께 갱신) */
export const WEB_VERSION = "1.6.0";

/** 웹 플래셔 릴리즈 노트 (GitHub) */
export const RELEASE_NOTES_URL = `${REPOSITORY_URL}/blob/${REPOSITORY_BRANCH}/web/RELEASE_NOTES.md`;

/** 프로젝트별 README 링크 (GitHub) */
export function readmeUrl(projectId: string): string {
  return `${REPOSITORY_URL}/blob/${REPOSITORY_BRANCH}/${encodeURIComponent(projectId)}/README.md`;
}
