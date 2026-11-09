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

/** 슬롯 하나를 화면에 그린다 (버퍼만 — 전송은 display.push()) */
void drawSlot(U8G2& u, const Slot& s, uint8_t idx, uint32_t nowMs);

/**
 * @brief 승인 화면 중 화면 idx 부분을 그린다 (4화면이 한 장처럼 이어짐)
 * @details 머리줄(반전 깜빡임): APPROVE? · 세션 이름 · 도구 · n/q + 대기 시간
 *          본문: 명령 요약을 화면당 21자씩 이어서 2줄
 *          아래: 각 버튼 위 라벨 — APPROVE · NEXT(대기 2개 이상) · DENY · LATER
 */
void drawApproval(U8G2& u, uint8_t idx, const Perm& p, uint32_t nowMs);

/** PC 데몬과 연결되지 않았을 때 화면 idx 의 안내 */
void drawOffline(U8G2& u, uint8_t idx, bool everConnected, uint32_t nowMs);

/** 애니메이션·깜빡임 때문에 주기적으로 다시 그려야 하는 상태인지 */
bool isAnimated(State st);

}  // namespace am

#endif
