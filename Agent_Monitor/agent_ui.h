// worynim@gmail.com
/**
 * @file agent_ui.h
 * @brief 개요 화면 렌더링 — OLED 한 장 = 세션 슬롯 하나
 * @details 흑백 화면이라 상태를 아이콘 모양 + 움직임으로 구분한다.
 *          대기=빈 원 · 생각 중=점 3개 · 실행 중=회전 스피너 · 승인/입력 대기=!/? + 화면 반전 깜빡임 ·
 *          완료=체크 + 굵은 테두리 · 오류=X
 */
#ifndef AGENT_UI_H
#define AGENT_UI_H

#include <U8g2lib.h>

#include "agent_state.h"

namespace am {

/**
 * @brief 슬롯 하나를 화면에 그린다 (버퍼만 — 전송은 display.push())
 * @details 빈 슬롯이면 그 화면 밑 버튼을 길게 눌렀을 때 실행되는 PC 명령(cmd)을 보여준다.
 */
void drawSlot(U8G2& u, const Slot& s, uint8_t idx, const CmdInfo& cmd, uint32_t nowMs);

/**
 * @brief 승인 화면 중 화면 idx 부분을 그린다 (4화면이 한 장처럼 이어짐)
 * @details 머리줄(반전 깜빡임): APPROVE? · 세션 이름 · 도구 · n/q + 대기 시간
 *          본문: 명령 요약을 화면당 21자씩 이어서 2줄
 *          아래: 각 버튼 위 라벨 — APPROVE · NEXT(대기 2개 이상) · DENY · LATER
 */
void drawApproval(U8G2& u, uint8_t idx, const Perm& p, uint32_t nowMs);

/**
 * @brief 상세 화면 중 화면 idx 부분을 그린다
 * @details 화면 0: 이름 · 상태 · 경과 · 권한 모드 · 하위 에이전트/오류 수
 *          화면 1: 최근 도구 4개 / 화면 2~3: 마지막 답변
 *          아래 라벨: BACK · MUTE(UNMUTE) · < PREV · NEXT >
 */
void drawDetail(U8G2& u, uint8_t idx, const Detail& d, bool muted, uint32_t nowMs);

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
