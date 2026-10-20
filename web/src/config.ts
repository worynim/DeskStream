/** DeskStream 저장소 정보 (README 링크 생성에 사용) */
export const REPOSITORY_URL = "https://github.com/worynim/DeskStream";
export const REPOSITORY_BRANCH = "main";

/** 프로젝트별 README 링크 (GitHub) */
export function readmeUrl(projectId: string): string {
  return `${REPOSITORY_URL}/blob/${REPOSITORY_BRANCH}/${encodeURIComponent(projectId)}/README.md`;
}
