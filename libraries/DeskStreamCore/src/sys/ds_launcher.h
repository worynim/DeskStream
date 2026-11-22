// worynim@gmail.com
/**
 * @file ds_launcher.h
 * @brief 런처(esp-launcher)로 돌아가기
 * @details 런처는 factory 파티션에, 앱은 ota_0 에 설치된다. 다음 부팅을 factory 로 정하고 재시작하면
 *          런처가 뜬다. 버튼 조합 대신 앱 메뉴에서 런처로 돌아갈 때 쓴다.
 *          (BTN1 을 누른 채 리셋해 2초 유지하는 방법은 부트로더가 처리하므로 앱과 상관없이 동작한다.)
 */
#ifndef DS_LAUNCHER_H
#define DS_LAUNCHER_H

namespace ds {

/**
 * 런처가 설치돼 있으면 다음 부팅을 런처로 정하고 재시작한다 (돌아오지 않음).
 * @return 런처가 없거나 부팅 파티션을 바꾸지 못하면 false (아무것도 바뀌지 않음)
 */
bool returnToLauncher();

}  // namespace ds

#endif
