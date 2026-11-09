// worynim@gmail.com
/**
 * @file ds_web_config.h
 * @brief 웹 설정 API — ConfigStore 필드 테이블로 GET/POST /api/config 를 자동 처리
 * @details
 *   · GET  /api/config → 테이블의 모든 필드를 JSON으로 (+ extraJson 훅)
 *   · FIELD_NO_WEB 필드는 GET/POST 모두 건너뛴다 (NVS 전용)
 *   · POST /api/config → 본문에 있는 키만 **테이블 순서대로** 파싱·검증하고, 값이 바뀐 필드만 적용
 *                        (onChange가 있으면 그 함수가, 없으면 저장소가 쓰고 setDirty)
 *                        → 적용 순서가 계약인 필드(예: font_slot 다음 font_name)는 테이블 순서로 정한다.
 *                        검증 실패 값은 로그만 남기고 건너뛴다. 본문이 없으면 400.
 *   WebServer는 스케치가 소유한다 — 프로젝트 전용 경로(페이지·업로드 등)는 같은 서버에 직접 등록한다.
 * @note 출처: Hangeul_Clock/web_manager.cpp handleGetConfig·handleSetConfig·jsonEscape (v2.10.0)
 */
#ifndef DS_WEB_CONFIG_H
#define DS_WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include "../store/ds_config.h"

namespace ds {

/** JSON 문자열 값 안에 들어갈 수 있도록 이스케이프한다 (UTF-8 바이트는 그대로 통과) */
String jsonEscape(const String& s);

class WebConfigApi {
public:
    /** GET 응답 끝(닫는 중괄호 앞)에 덧붙일 내용 — 예: `,"slot_names":[...]` */
    typedef void (*ExtraJson)(String& json);
    /**
     * POST의 필드 테이블 처리가 끝난 뒤 한 번 부른다 (본문 전달) — 예: 화면 강제 갱신,
     * 테이블에 없는 계산 키(여러 필드를 묶은 값) 처리. 파싱은 ds_json_lite의 jsonGet*을 쓴다.
     */
    typedef void (*AfterPost)(const char* body);

    void begin(WebServer& server, ConfigStore& store, ExtraJson extra = nullptr, AfterPost after = nullptr);

private:
    void handleGet();
    void handlePost();

    WebServer* _server = nullptr;
    ConfigStore* _store = nullptr;
    ExtraJson _extra = nullptr;
    AfterPost _after = nullptr;
};

}  // namespace ds

#endif
