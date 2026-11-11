// worynim@gmail.com
/**
 * @file agent_ui.h
 * @brief 개요 화면 렌더링 — OLED 한 장 = 세션 슬롯 하나
 * @details 흑백 화면이라 상태를 아이콘 모양 + 움직임으로 구분한다.
 *          대기=빈 원 · 생각 중=점 3개 · 실행 중=회전 스피너 · 승인/입력 대기=!/? + 화면 반전 깜빡임 ·
 *          완료=체크 + 구분선 아래 반전 · 오류=X
 */
#ifndef AGENT_UI_H
#define AGENT_UI_H

#include <U8g2lib.h>

#include "agent_state.h"

namespace am {

constexpr int kMaxMsgPages = 4;   ///< 상세 화면 마지막 답변 최대 페이지 수 (한 페이지 = 9줄)

/**
 * @brief 슬롯 하나를 화면에 그린다 (버퍼만 — 전송은 display.push())
 * @details 빈 슬롯이면 마지막 두 화면(idx 2·3)에는 Claude Code 사용량(5시간 · 주간)과 리셋까지 남은 시간을,
 *          나머지 화면에는 슬롯 번호만 보여준다. usage = 구간 kUsageWindows 개 배열.
 */
void drawSlot(U8G2& u, const Slot& s, uint8_t idx, const Usage* usage, uint32_t nowMs);

/**
 * @brief 승인 화면 중 화면 idx 부분을 그린다 (4화면이 한 장처럼 이어짐)
 * @details 머리줄(반전 깜빡임): APPROVE? · 세션 이름 · 도구 · n/q + 대기 시간
 *          본문: 명령 요약을 화면당 한 줄(128px)씩 이어서 2줄
 *          아래: 각 버튼 위 라벨 — APPROVE · NEXT(대기 2개 이상) · DENY · LATER
 */
void drawApproval(U8G2& u, uint8_t idx, const Perm& p, uint32_t nowMs);

/**
 * @brief 상세 화면 중 화면 idx 부분을 그린다
 * @details 화면 0: 이름 · 상태 · 경과 · 권한 모드 · 하위 에이전트/오류 수
 *          화면 1: 최근 도구 4개 / 화면 2~3: 마지막 답변
 *          아래 라벨: BACK · CLOSE(세션 닫기) · < PREV · NEXT > (마지막 답변 이전/다음 페이지 — 넘길 수 있을 때만)
 * @param page 마지막 답변 페이지 (0부터, 범위를 넘으면 마지막 페이지로 맞춘다)
 */
void drawDetail(U8G2& u, uint8_t idx, const Detail& d, uint8_t page, uint32_t nowMs);

/** 마지막으로 그린 상세 화면의 마지막 답변 페이지 수 (1~kMaxMsgPages) — 버튼으로 넘길 수 있는 범위 */
uint8_t detailPageCount();

/** PC 명령 진행·결과 띠를 화면 아래에 덧그린다 (표시할 것이 없으면 아무것도 안 함) */
void drawToast(U8G2& u, const CmdToast& t, uint32_t nowMs);

/** 화면 아래에 반전 띠로 짧은 안내를 덧그린다 */
void drawNotice(U8G2& u, const char* text);

/** 음소거 표시 (오른쪽 위 작은 스피커 + X) */
void drawMuteIcon(U8G2& u);

/** PC 데몬과 연결되지 않았을 때 화면 idx 의 안내 */
void drawOffline(U8G2& u, uint8_t idx, bool everConnected, uint32_t nowMs);

/** 애니메이션·깜빡임 때문에 주기적으로 다시 그려야 하는 상태인지 */
bool isAnimated(State st);

}  // namespace am

#endif
