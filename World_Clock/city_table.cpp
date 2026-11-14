// worynim@gmail.com
/**
 * @file city_table.cpp
 * @brief 도시 표 데이터와 목록 이동 구현
 */
#include "city_table.h"

namespace {

// 서머타임 규칙 {월, 몇 번째(5 = 마지막), 요일(0 = 일), 날 이동, 0시부터 분, UTC 기준 여부}
const DstRule kRuleUS = {{3, 2, 0, 0, 120, false}, {11, 1, 0, 0, 120, false}};     // 3월 둘째 일 02:00 ~ 11월 첫째 일 02:00
const DstRule kRuleEU = {{3, 5, 0, 0, 60, true}, {10, 5, 0, 0, 60, true}};         // 3월 마지막 일 ~ 10월 마지막 일, 01:00 UTC
const DstRule kRuleAU = {{10, 1, 0, 0, 120, false}, {4, 1, 0, 0, 180, false}};     // 10월 첫째 일 02:00 ~ 4월 첫째 일 03:00
const DstRule kRuleNZ = {{9, 5, 0, 0, 120, false}, {4, 1, 0, 0, 180, false}};      // 9월 마지막 일 02:00 ~ 4월 첫째 일 03:00
const DstRule kRuleCL = {{9, 1, 6, 0, 1440, false}, {4, 1, 6, 0, 1440, false}};    // 9월 첫째 토 24:00 ~ 4월 첫째 토 24:00
const DstRule kRuleEG = {{4, 5, 5, 0, 0, false}, {10, 5, 4, 0, 1440, false}};      // 4월 마지막 금 00:00 ~ 10월 마지막 목 24:00
const DstRule kRuleIL = {{3, 5, 0, -2, 120, false}, {10, 5, 0, 0, 120, false}};    // 3월 마지막 일요일 앞 금 02:00 ~ 10월 마지막 일 02:00

enum ZoneId : uint8_t { Z_HST, Z_LINE, Z_AK, Z_PT, Z_MT, Z_AZ, Z_CT, Z_M6, Z_ET, Z_M5, Z_AT, Z_M4, Z_CL, Z_NST, Z_M3, Z_UTC, Z_WET, Z_CET, Z_P1, Z_EET, Z_EG, Z_IL, Z_P2, Z_P3, Z_P330, Z_P4, Z_P430, Z_P5, Z_P530, Z_P545, Z_P6, Z_P630, Z_P7, Z_P8, Z_P9, Z_P930, Z_ACST, Z_P10, Z_AEST, Z_P11, Z_NZ, Z_P12, Z_P13, Z_COUNT };

// {표준시 UTC 차이(분), 서머타임 규칙} — ZoneId 순서
const TzInfo kZones[Z_COUNT] = {
    {-600, nullptr},   // Z_HST
    {840, nullptr},   // Z_LINE
    {-540, &kRuleUS},   // Z_AK
    {-480, &kRuleUS},   // Z_PT
    {-420, &kRuleUS},   // Z_MT
    {-420, nullptr},   // Z_AZ
    {-360, &kRuleUS},   // Z_CT
    {-360, nullptr},   // Z_M6
    {-300, &kRuleUS},   // Z_ET
    {-300, nullptr},   // Z_M5
    {-240, &kRuleUS},   // Z_AT
    {-240, nullptr},   // Z_M4
    {-240, &kRuleCL},   // Z_CL
    {-210, &kRuleUS},   // Z_NST
    {-180, nullptr},   // Z_M3
    {0, nullptr},   // Z_UTC
    {0, &kRuleEU},   // Z_WET
    {60, &kRuleEU},   // Z_CET
    {60, nullptr},   // Z_P1
    {120, &kRuleEU},   // Z_EET
    {120, &kRuleEG},   // Z_EG
    {120, &kRuleIL},   // Z_IL
    {120, nullptr},   // Z_P2
    {180, nullptr},   // Z_P3
    {210, nullptr},   // Z_P330
    {240, nullptr},   // Z_P4
    {270, nullptr},   // Z_P430
    {300, nullptr},   // Z_P5
    {330, nullptr},   // Z_P530
    {345, nullptr},   // Z_P545
    {360, nullptr},   // Z_P6
    {390, nullptr},   // Z_P630
    {420, nullptr},   // Z_P7
    {480, nullptr},   // Z_P8
    {540, nullptr},   // Z_P9
    {570, nullptr},   // Z_P930
    {570, &kRuleAU},   // Z_ACST
    {600, nullptr},   // Z_P10
    {600, &kRuleAU},   // Z_AEST
    {660, nullptr},   // Z_P11
    {720, &kRuleNZ},   // Z_NZ
    {720, nullptr},   // Z_P12
    {780, nullptr},   // Z_P13
};

// {ID, 한글 이름, 영문 이름, 경도, 위도, 시간대} — 경도 순서(서→동). ID 는 바꾸지 않는다
const City kCities[] = {
    {  0, "누쿠알로파", "Nuku'alofa", -175, -21, Z_P13},
    {  1, "호놀룰루", "Honolulu", -158, 21, Z_HST},
    {  2, "키리티마티", "Kiritimati", -157, 2, Z_LINE},
    {  3, "앵커리지", "Anchorage", -150, 61, Z_AK},
    {  4, "밴쿠버", "Vancouver", -123, 49, Z_AZ},      // 2026-11 부터 서머타임 없이 UTC-7 고정 (tzdata 2026f)
    {  5, "시애틀", "Seattle", -122, 48, Z_PT},
    {  6, "샌프란시스코", "San Francisco", -122, 38, Z_PT},
    {  7, "로스앤젤레스", "Los Angeles", -118, 34, Z_PT},
    {  8, "라스베이거스", "Las Vegas", -115, 36, Z_PT},
    {  9, "캘거리", "Calgary", -114, 51, Z_M6},        // 2026-11 부터 서머타임 없이 UTC-6 고정 (tzdata 2026f)
    { 10, "피닉스", "Phoenix", -112, 33, Z_AZ},
    { 11, "덴버", "Denver", -105, 40, Z_MT},
    { 12, "멕시코시티", "Mexico City", -99, 19, Z_M6},
    { 13, "댈러스", "Dallas", -97, 33, Z_CT},
    { 14, "휴스턴", "Houston", -95, 30, Z_CT},
    { 15, "시카고", "Chicago", -88, 42, Z_CT},
    { 16, "애틀랜타", "Atlanta", -84, 34, Z_ET},
    { 17, "마이애미", "Miami", -80, 26, Z_ET},
    { 18, "파나마시티", "Panama City", -80, 9, Z_M5},
    { 19, "토론토", "Toronto", -79, 44, Z_ET},
    { 20, "리마", "Lima", -77, -12, Z_M5},
    { 21, "워싱턴", "Washington", -77, 39, Z_ET},
    { 22, "보고타", "Bogota", -74, 5, Z_M5},
    { 23, "뉴욕", "New York", -74, 41, Z_ET},
    { 24, "보스턴", "Boston", -71, 42, Z_ET},
    { 25, "산티아고", "Santiago", -71, -33, Z_CL},
    { 26, "라파스", "La Paz", -68, -17, Z_M4},
    { 27, "카라카스", "Caracas", -67, 10, Z_M4},
    { 28, "핼리팩스", "Halifax", -64, 45, Z_AT},
    { 29, "부에노스아이레스", "Buenos Aires", -58, -35, Z_M3},
    { 30, "세인트존스", "St. John's", -53, 48, Z_NST},
    { 31, "상파울루", "Sao Paulo", -47, -24, Z_M3},
    { 32, "리우데자네이루", "Rio de Janeiro", -43, -23, Z_M3},
    { 33, "레이캬비크", "Reykjavik", -22, 64, Z_UTC},
    { 34, "다카르", "Dakar", -17, 15, Z_UTC},
    { 35, "리스본", "Lisbon", -9, 39, Z_WET},
    { 36, "더블린", "Dublin", -6, 53, Z_WET},
    { 37, "마드리드", "Madrid", -4, 40, Z_CET},
    { 38, "아크라", "Accra", 0, 6, Z_UTC},
    { 39, "런던", "London", 0, 51, Z_WET},
    { 40, "파리", "Paris", 2, 49, Z_CET},
    { 41, "바르셀로나", "Barcelona", 2, 41, Z_CET},
    { 42, "알제", "Algiers", 3, 37, Z_P1},
    { 43, "라고스", "Lagos", 3, 6, Z_P1},
    { 44, "브뤼셀", "Brussels", 4, 51, Z_CET},
    { 45, "암스테르담", "Amsterdam", 5, 52, Z_CET},
    { 46, "취리히", "Zurich", 9, 47, Z_CET},
    { 47, "오슬로", "Oslo", 11, 60, Z_CET},
    { 48, "로마", "Rome", 12, 42, Z_CET},
    { 49, "베를린", "Berlin", 13, 53, Z_CET},
    { 50, "코펜하겐", "Copenhagen", 13, 56, Z_CET},
    { 51, "프라하", "Prague", 14, 50, Z_CET},
    { 52, "빈", "Vienna", 16, 48, Z_CET},
    { 53, "스톡홀름", "Stockholm", 18, 59, Z_CET},
    { 54, "케이프타운", "Cape Town", 18, -34, Z_P2},
    { 55, "부다페스트", "Budapest", 19, 47, Z_CET},
    { 56, "바르샤바", "Warsaw", 21, 52, Z_CET},
    { 57, "아테네", "Athens", 24, 38, Z_EET},
    { 58, "헬싱키", "Helsinki", 25, 60, Z_EET},
    { 59, "부쿠레슈티", "Bucharest", 26, 44, Z_EET},
    { 60, "요하네스버그", "Johannesburg", 28, -26, Z_P2},
    { 61, "이스탄불", "Istanbul", 29, 41, Z_P3},
    { 62, "키이우", "Kyiv", 31, 50, Z_EET},
    { 63, "카이로", "Cairo", 31, 30, Z_EG},
    { 64, "예루살렘", "Jerusalem", 35, 32, Z_IL},
    { 65, "나이로비", "Nairobi", 37, -1, Z_P3},
    { 66, "모스크바", "Moscow", 38, 56, Z_P3},
    { 67, "아디스아바바", "Addis Ababa", 39, 9, Z_P3},
    { 68, "바그다드", "Baghdad", 44, 33, Z_P3},
    { 69, "리야드", "Riyadh", 47, 25, Z_P3},
    { 70, "바쿠", "Baku", 50, 40, Z_P4},
    { 71, "테헤란", "Tehran", 51, 36, Z_P330},
    { 72, "도하", "Doha", 52, 25, Z_P3},
    { 73, "두바이", "Dubai", 55, 25, Z_P4},
    { 74, "카라치", "Karachi", 67, 25, Z_P5},
    { 75, "카불", "Kabul", 69, 35, Z_P430},
    { 76, "타슈켄트", "Tashkent", 69, 41, Z_P5},
    { 77, "뭄바이", "Mumbai", 73, 19, Z_P530},
    { 78, "알마티", "Almaty", 77, 43, Z_P5},
    { 79, "뉴델리", "New Delhi", 77, 29, Z_P530},
    { 80, "벵갈루루", "Bengaluru", 78, 13, Z_P530},
    { 81, "콜롬보", "Colombo", 80, 7, Z_P530},
    { 82, "카트만두", "Kathmandu", 85, 28, Z_P545},
    { 83, "콜카타", "Kolkata", 88, 23, Z_P530},
    { 84, "다카", "Dhaka", 90, 24, Z_P6},
    { 85, "양곤", "Yangon", 96, 17, Z_P630},
    { 86, "방콕", "Bangkok", 101, 14, Z_P7},
    { 87, "쿠알라룸푸르", "Kuala Lumpur", 102, 3, Z_P8},
    { 88, "싱가포르", "Singapore", 104, 1, Z_P8},
    { 89, "하노이", "Hanoi", 106, 21, Z_P7},
    { 90, "호찌민", "Ho Chi Minh", 107, 11, Z_P7},
    { 91, "자카르타", "Jakarta", 107, -6, Z_P7},
    { 92, "울란바토르", "Ulaanbaatar", 107, 48, Z_P8},
    { 93, "홍콩", "Hong Kong", 114, 22, Z_P8},
    { 94, "퍼스", "Perth", 116, -32, Z_P8},
    { 95, "베이징", "Beijing", 116, 40, Z_P8},
    { 96, "마닐라", "Manila", 121, 15, Z_P8},
    { 97, "상하이", "Shanghai", 121, 31, Z_P8},
    { 98, "타이베이", "Taipei", 122, 25, Z_P8},
    { 99, "서울", "Seoul", 127, 38, Z_P9},
    {100, "부산", "Busan", 129, 35, Z_P9},
    {101, "다윈", "Darwin", 131, -12, Z_P930},
    {102, "블라디보스토크", "Vladivostok", 132, 43, Z_P10},
    {103, "오사카", "Osaka", 136, 35, Z_P9},
    {104, "애들레이드", "Adelaide", 139, -35, Z_ACST},
    {105, "도쿄", "Tokyo", 140, 36, Z_P9},
    {106, "괌", "Guam", 145, 13, Z_P10},
    {107, "멜버른", "Melbourne", 145, -38, Z_AEST},
    {108, "시드니", "Sydney", 151, -34, Z_AEST},
    {109, "브리즈번", "Brisbane", 153, -27, Z_P10},
    {110, "누메아", "Noumea", 166, -22, Z_P11},
    {111, "오클랜드", "Auckland", 175, -37, Z_NZ},
    {112, "수바", "Suva", 178, -18, Z_P12},
};

const int kCityCount = sizeof(kCities) / sizeof(kCities[0]);

}  // namespace

const uint8_t kDefaultCityIds[4] = {99, 39, 23, 108};

int cityCount() { return kCityCount; }

const City& cityAt(int index) { return kCities[index]; }

const TzInfo& cityTz(const City& city) { return kZones[city.zone]; }

int cityIndexOfId(uint8_t id) {
    for (int i = 0; i < kCityCount; i++) {
        if (kCities[i].id == id) return i;
    }
    return -1;
}

int cityStep(int index, int dir) { return (index + dir + kCityCount) % kCityCount; }

int cityJumpZone(int index, int dir, int64_t utc) {
    const int from = utcOffsetMinutes(utc, cityTz(kCities[index]));
    int i = index;
    for (int n = 0; n < kCityCount; n++) {
        i = cityStep(i, dir);
        if (utcOffsetMinutes(utc, cityTz(kCities[i])) != from) break;
    }
    return i;
}

int cityMapX(const City& city) { return (city.lon + 180) * 128 / 360; }

int cityMapY(const City& city) { return (90 - city.lat) * 64 / 180; }
