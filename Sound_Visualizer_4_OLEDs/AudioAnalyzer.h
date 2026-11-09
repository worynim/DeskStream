// worynim@gmail.com
/**
 * @file AudioAnalyzer.h
 * @brief 오디오 데이터 분석 클래스 및 FFT 상수 선언
 * @details 고성능 주파수 분석을 위한 클래스 인터페이스 및 밴드 필터링 매개변수 정의
 */
#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <Arduino.h>
#include "esp_dsp.h"
#include <LovyanGFX.hpp>
#include "esp_adc/adc_continuous.h"
#include "config.h"



/**
 * @brief ADC DMA 기반 FFT 분석 및 렌더링 클래스
 * 
 * ESP32-C3의 ADC Continuous Driver(DMA)를 사용하여 하드웨어적으로 
 * 고속 샘플링(최대 83kHz)을 수행하고 주파수 대역을 시각화합니다.
 */
class AudioAnalyzer {
public:
    AudioAnalyzer(uint16_t samples, float samplingFreq, uint16_t numBands);
    ~AudioAnalyzer();
    void begin();
    bool available();  // DMA 버퍼 데이터를 읽어 분석 준비 완료 여부 확인
    void process();    // FFT 연산 및 주파수 추출
    void render(LGFX_Sprite& canvas); // 가상 캔버스에 시각화 결과 렌더링 (스펙트럼 막대)
    void renderMirror(LGFX_Sprite& canvas);   // 중앙선 기준 위아래 대칭 스펙트럼 렌더링
    void renderSpectrogram(LGFX_Sprite& canvas); // 시간(가로)×주파수(세로) 스크롤 스펙트로그램 (디더링)
    void resetHistory() { spectro_reset = true; } // 스펙트로그램 누적 화면 초기화 요청 (모드 진입·화면 덮어쓴 뒤)
    void renderParticles(LGFX_Sprite& canvas); // 주파수 위치별로 떨어져 바닥에서 튕기는 파티클
    void renderDotMatrix(LGFX_Sprite& canvas); // LED 도트 매트릭스 스타일 이퀄라이저 + 박자 플래시
    void renderWaveform(LGFX_Sprite& canvas); // 오실로스코프 스타일 파형 렌더링
    float getBandAmplitude(int band_idx); // 매핑된 밴드 진폭 계산 (공용)
    
    uint32_t getLastFftTime() const { return last_fft_time; }
    uint32_t getLastRenderTime() const { return last_render_time; }

    const float* getvReal() const { return vReal; }
    void setNoiseFloor(int band, float value) { if(band >= 0 && band < NUM_BANDS) noise_floor[band] = value; }

private:
    struct BandMapping {
        float startBin;
        float endBin;
    };

    void calculateMappings(); // 매핑 테이블 미리 계산
    void updateBands();       // 밴드 진폭 → 막대 높이·피크 갱신 (막대 계열 모드 공용)
    int  bassLevel() const;   // 하위 BASS_BANDS개 밴드의 최대 세기 (0~CANVAS_HEIGHT)
    bool detectBeat(int bass); // 저역 세기로 박자 판정 (평균 대비 급상승, 쿨다운 포함)

    uint16_t SAMPLES;
    float SAMPLING_FREQ;
    uint16_t NUM_BANDS;
    float* vReal;
    int16_t* fft_data_int;
    float* window_data;
    float* noise_floor;
    BandMapping* band_mappings; // 밴드별 Bin 매핑 테이블
    float* waveformBuf;         // 오실로스코프용 시간 영역 파형 버퍼 (DC 제거 후)
    
    // 박자 판정 관련 (도트 매트릭스)
    float bass_avg = 0;                 // 저역 세기의 이동 평균 (박자 판정 기준)
    int beat_cooldown = 0;              // 박자 재판정까지 남은 프레임
    // 파티클 관련 (x·y·vx·vy는 1/16 픽셀 단위 고정소수점)
    struct Particle { int16_t x; int16_t y; int16_t vx; int16_t vy; bool active; };
    Particle particles[PARTICLE_MAX] = {};
    int flash_left = 0;                 // 도트 매트릭스 박자 플래시 남은 프레임

    // ADC DMA 관련
    adc_continuous_handle_t adc_handle;
    uint8_t* adc_raw_buf;

    // 시각화 관련
    int* peak;
    int* peak_hold;
    int* smooth_bar;
    int* band_level;            // 밴드별 현재 프레임 세기 (0~CANVAS_HEIGHT, 평활 전)
    bool spectro_reset = true;  // true면 다음 스펙트로그램 렌더 때 화면을 비움
    uint32_t spectro_col = 0;   // 누적 열 번호 (디더 패턴을 내용에 고정)
    float agc_max_amp = AGC_INITIAL_MAX_AMP;
    uint32_t last_fft_time = 0;
    uint32_t last_render_time = 0;
    uint32_t last_frame_time_us = 0;
};

#endif
