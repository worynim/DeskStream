// worynim@gmail.com
/**
 * @file AudioAnalyzer.cpp
 * @brief 고속 FFT 기반 오디오 스펙트럼 분석 엔진 구현
 * @details I2S DMA 데이터 수집, 가우스 가중치 적용 FFT 및 주파수 밴드 평활화 처리
 */
#include "AudioAnalyzer.h"
#include "soc/soc_caps.h"

// 메인 스케치에 정의된 디버그 모드 변수 참조
extern int DEBUG_MODE;

#ifndef SOC_ADC_DIGI_RESULT_BYTES
#define SOC_ADC_DIGI_RESULT_BYTES 4
#endif

AudioAnalyzer::AudioAnalyzer(uint16_t samples, float samplingFreq, uint16_t numBands) 
    : SAMPLES(samples), SAMPLING_FREQ(samplingFreq), NUM_BANDS(numBands),
      adc_handle(NULL) {
    vReal = new float[samples];
    fft_data_int = new int16_t[samples * 2];
    window_data = new float[samples];
    peak = new int[numBands];
    peak_hold = new int[numBands];
    smooth_bar = new int[numBands];
    band_level = new int[numBands];
    noise_floor = new float[numBands];
    band_mappings = new BandMapping[numBands]; // 매핑 테이블 메모리 할당
    waveformBuf = new float[samples];           // 오실로스코프용 파형 버퍼
    
    // ADC DMA 결과 버퍼 할당 (SAMPLES개 × 4바이트)
    adc_raw_buf = new uint8_t[samples * SOC_ADC_DIGI_RESULT_BYTES];
    
    // 초기화 (사용자가 설정한 바 개수만큼)
    for (int i = 0; i < numBands; i++) {
        peak[i] = 0;
        peak_hold[i] = 0;
        smooth_bar[i] = 0;
        band_level[i] = 0;
        noise_floor[i] = DEFAULT_NOISE_FLOOR; // 기본 노이즈 플로어
        band_mappings[i] = {0, 0};
    }
}

AudioAnalyzer::~AudioAnalyzer() {
    delete[] vReal;
    delete[] fft_data_int;
    delete[] window_data;
    delete[] peak;
    delete[] peak_hold;
    delete[] smooth_bar;
    delete[] band_level;
    delete[] noise_floor;
    delete[] band_mappings;
    delete[] waveformBuf;
    delete[] adc_raw_buf;
}

void AudioAnalyzer::calculateMappings() {
    float bin_step = SAMPLING_FREQ / SAMPLES;

    if (MAPPING_MODE == 1) {
        // [1] 로그(Octave) 매핑 계산
        for (int i = 0; i < NUM_BANDS; i++) {
            float f_low = LOG_MIN_FREQ * pow(LOG_MAX_FREQ / LOG_MIN_FREQ, (float)i / NUM_BANDS);
            float f_high = LOG_MIN_FREQ * pow(LOG_MAX_FREQ / LOG_MIN_FREQ, (float)(i + 1) / NUM_BANDS);
            
            band_mappings[i].startBin = f_low / bin_step;
            band_mappings[i].endBin = f_high / bin_step;
        }
    } else if (MAPPING_MODE == 2) {
        // [2] 거듭제곱(Power/Box-Cox) 매핑 계산
        // f_i = f_min + (f_max - f_min) * (i/N)^P
        for (int i = 0; i < NUM_BANDS; i++) {
            float norm_low = (float)i / NUM_BANDS;
            float norm_high = (float)(i + 1) / NUM_BANDS;
            
            float f_low = LOG_MIN_FREQ + (LOG_MAX_FREQ - LOG_MIN_FREQ) * pow(norm_low, MAPPING_POWER);
            float f_high = LOG_MIN_FREQ + (LOG_MAX_FREQ - LOG_MIN_FREQ) * pow(norm_high, MAPPING_POWER);
            
            band_mappings[i].startBin = f_low / bin_step;
            band_mappings[i].endBin = f_high / bin_step;
        }
    } else {
        // [3] 기존 선형(Linear) 매핑 계산
        float skipBins = LOG_MIN_FREQ / bin_step;
        for (int i = 0; i < NUM_BANDS; i++) {
            band_mappings[i].startBin = skipBins + (float)i;
            band_mappings[i].endBin = skipBins + (float)(i + 1);
        }
    }
}

void AudioAnalyzer::begin() {
    // 1. DMA 핸들 생성
    adc_continuous_handle_cfg_t handle_cfg = {};
    handle_cfg.max_store_buf_size = SAMPLES * SOC_ADC_DIGI_RESULT_BYTES * ADC_DMA_BUF_COUNT;  // 내부 링버퍼 (프레임 4개분)
    handle_cfg.conv_frame_size  = SAMPLES * SOC_ADC_DIGI_RESULT_BYTES;        // 1프레임 = SAMPLES개 샘플
    
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    // 2. ADC 채널/패턴 설정 (GPIO 0, Unit 1)
    adc_digi_pattern_config_t adc_pattern = {};
    adc_pattern.atten    = ADC_ATTEN_DB_11;
    adc_pattern.channel  = ADC_CHANNEL_0;
    adc_pattern.unit     = ADC_UNIT_1;
    adc_pattern.bit_width = ADC_BITWIDTH_12;

    // 3. 연속 변환 드라이버 설정
    adc_continuous_config_t adc_cfg = {};
    adc_cfg.sample_freq_hz = (uint32_t)SAMPLING_FREQ;
    adc_cfg.conv_mode      = ADC_CONV_SINGLE_UNIT_1;
    adc_cfg.format         = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
    adc_cfg.pattern_num    = 1;
    adc_cfg.adc_pattern    = &adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &adc_cfg));

    // 4. 매핑 테이블 계산
    calculateMappings();

    // 5. ESP-DSP FFT 초기화 및 사용자 정의 윈도우 생성
    dsps_fft2r_init_sc16(NULL, SAMPLES);
    for (int i = 0; i < SAMPLES; i++) {
        float ratio = (float)i / (SAMPLES - 1);
        float w = 1.0f; // 기본 Rectangle(0)
        switch (FFT_WINDOW_TYPE) {
            case 1: w = 0.54f - 0.46f * cos(2.0f * PI * ratio); break; // Hamming
            case 2: w = 0.5f * (1.0f - cos(2.0f * PI * ratio)); break; // Hann
            case 3: w = 0.42f - 0.5f * cos(2.0f * PI * ratio) + 0.08f * cos(4.0f * PI * ratio); break; // Blackman
            case 4: w = 0.35875f - 0.48829f * cos(2.0f * PI * ratio) + 0.14128f * cos(4.0f * PI * ratio) - 0.01168f * cos(6.0f * PI * ratio); break; // Blackman-Harris
            case 5: w = 0.2155789f - 0.41663158f * cos(2.0f * PI * ratio) + 0.277263158f * cos(4.0f * PI * ratio) - 0.083578947f * cos(6.0f * PI * ratio) + 0.006947368f * cos(8.0f * PI * ratio); break; // Flat-top
        }
        window_data[i] = w;
    }

    // 6. ADC 시작
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    const char* mode_name = (MAPPING_MODE == 0) ? "선형" : (MAPPING_MODE == 1 ? "로그" : "거듭제곱");
    Serial.printf("[ADC] DMA 모드 및 %s 매핑 준비 완료\n", mode_name);
}

bool AudioAnalyzer::available() {
    uint32_t needed = SAMPLES * SOC_ADC_DIGI_RESULT_BYTES;
    uint32_t ret_num = 0;
    if (!adc_handle || !adc_raw_buf) return false;

    // DMA 버퍼에서 SAMPLES개 분량의 데이터 읽기 (비차단, timeout=0)
    esp_err_t ret = adc_continuous_read(adc_handle, adc_raw_buf, needed, &ret_num, 0);
    
    if (ret == ESP_OK && ret_num >= needed) {
        // DMA 원시 데이터를 vReal 배열로 변환
        uint32_t* raw_data = (uint32_t*)adc_raw_buf;
        for (int i = 0; i < SAMPLES; i++) {
            vReal[i] = (float)(raw_data[i] & 0x0FFF);  // 하위 12비트 추출
        }
        return true;
    }
    return false;
}

void AudioAnalyzer::process() {
    // 루프 간격 측정 (참고용 - DMA 수집 + FFT + 렌더 + 디바이스 전송 포함 전체 시간)
    uint32_t now_us = micros();
    uint32_t loop_interval_us = now_us - last_frame_time_us;
    last_frame_time_us = now_us;
    
    if (DEBUG_MODE == 3) {
        // 하드웨어가 DMA로 샘플링을 보장하므로 SAMPLING_FREQ는 신뢰할 수 있는 값입니다.
        float max_freq = (float)NUM_BANDS * SAMPLING_FREQ / SAMPLES;
        Serial.printf("[DIAG] Loop Interval: %lu us | Configured Fs: %u Hz | Display Range: 0-%.0f Hz (%u bars)\n",
                      loop_interval_us, (uint32_t)SAMPLING_FREQ, max_freq, NUM_BANDS);
    }

    uint32_t start = micros();
    
    // 1. DC 제거 (평균값 측정)
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) sum += (long)vReal[i];
    int mean = sum / SAMPLES;

    // 2. 값 증폭(<<4) 및 반감 보정용 윈도우 적용
    for (int i = 0; i < SAMPLES; i++) {
        int val = (int)vReal[i] - mean;
        waveformBuf[i] = (float)val;  // 오실로스코프용 DC 제거 파형 저장
        fft_data_int[i * 2 + 0] = (int16_t)((val << 4) * window_data[i]);
        fft_data_int[i * 2 + 1] = 0;
    }
    
    // 3. 고속 정수형 FFT 
    dsps_fft2r_sc16(fft_data_int, SAMPLES);
    dsps_bit_rev_sc16(fft_data_int, SAMPLES);

    // 4. 고속 진폭 근사 계산 및 스케일 롤백
    float scale_factor = (float)SAMPLES / 16.0f;
    for (int i = 0; i < SAMPLES / 2; i++) {
        long re = abs(fft_data_int[i * 2 + 0]);
        long im = abs(fft_data_int[i * 2 + 1]);
        
        float mag = (float)((re > im) ? (re + (im >> 1)) : (im + (re >> 1)));
        vReal[i] = mag * scale_factor;
    }
    last_fft_time = micros() - start;
}

/**
 * @brief 밴드 진폭 → 막대 높이(smooth_bar, 0~CANVAS_HEIGHT)·피크(peak) 갱신 (막대 계열 모드 공용)
 * @details 노이즈 차감 → AGC → 상승 즉시/하강 BAR_DROP_SPEED → 피크 홀드·낙하
 */
void AudioAnalyzer::updateBands() {
    uint16_t num_bands = NUM_BANDS;
    float current_frame_max = 0;
    float band_amps[NUM_BANDS]; // 노이즈 차감 후 실제 진폭 캐시

    // [Pass 1] 모든 밴드의 매핑 진폭 계산 및 AGC 기준값 탐색
    for (int i = 0; i < num_bands; i++) {
        float amp = getBandAmplitude(i);
        float threshold = noise_floor[i];
        if (amp < threshold) amp = 0; else amp -= threshold;
        band_amps[i] = amp;
        if (amp > current_frame_max) current_frame_max = amp;
    }

    // AGC (Auto Gain Control) — 매핑된 밴드 진폭 기준
    if (current_frame_max > agc_max_amp) agc_max_amp = current_frame_max;
    else agc_max_amp = (agc_max_amp * AGC_DECAY_RATE) + (current_frame_max * (1.0f - AGC_DECAY_RATE));

    if (agc_max_amp < AGC_MIN_AMPLITUDE) agc_max_amp = AGC_MIN_AMPLITUDE;

    // [Pass 2] 막대 높이·피크 갱신
    for (int i = 0; i < num_bands; i++) {
        int target_height = map((long)band_amps[i], 0, (long)agc_max_amp, 0, CANVAS_HEIGHT);
        target_height = constrain(target_height, 0, CANVAS_HEIGHT);
        band_level[i] = target_height;

        if (target_height > smooth_bar[i]) smooth_bar[i] = target_height;
        else {
            if (smooth_bar[i] > 0) smooth_bar[i] -= BAR_DROP_SPEED;
            if (smooth_bar[i] < 0) smooth_bar[i] = 0;
        }

        if (target_height >= peak[i]) {
            peak[i] = target_height;
            peak_hold[i] = PEAK_HOLD_TIME;
        } else {
            if (peak_hold[i] > 0) {
                peak_hold[i]--;
            } else {
                if (peak[i] > 0) peak[i] -= PEAK_DROP_SPEED;
                if (peak[i] < 0) peak[i] = 0;
            }
        }
    }
}

void AudioAnalyzer::render(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();

    canvas.clear();
    updateBands();

    int band_width = CANVAS_WIDTH / NUM_BANDS;
    for (int i = 0; i < NUM_BANDS; i++) {
        int bar_height = smooth_bar[i];
        int x = i * band_width;
        if (bar_height > 0) canvas.fillRect(x, CANVAS_HEIGHT - bar_height, band_width - 1, bar_height, COLOR_WHITE);

        // 피크(Peak) 도트 렌더링
        int peak_y = CANVAS_HEIGHT - peak[i] - 1;
        if (peak_y < 0) peak_y = 0; else if (peak_y > (CANVAS_HEIGHT - 2)) peak_y = (CANVAS_HEIGHT - 2);
        canvas.fillRect(x, peak_y, band_width - 1, PEAK_DOT_HEIGHT, COLOR_WHITE);
    }

    last_render_time = micros() - start_time;
}

/**
 * @brief 스펙트로그램 — 가로=시간(오른쪽이 최신), 세로=주파수(아래가 저음), 세기=디더링 점 밀도
 * @details 1비트 캔버스 버퍼를 1픽셀 왼쪽으로 밀고 맨 오른쪽 열만 새로 그린다.
 *          세기(0~CANVAS_HEIGHT)는 4x4 Bayer 디더로 점 밀도로 바꾸며, 패턴은 누적 열 번호에 고정되어 스크롤돼도 흔들리지 않는다.
 *          버퍼 형식: 픽셀 (x, y) = 바이트 (y*CANVAS_WIDTH/8 + x/8)의 비트 (7 - x%8).
 */
void AudioAnalyzer::renderSpectrogram(LGFX_Sprite& canvas) {
    static const uint8_t BAYER4[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
    const int stride = CANVAS_WIDTH / 8;
    uint32_t start_time = micros();

    if (spectro_reset) {
        canvas.clear();
        spectro_reset = false;
    }
    updateBands();

    uint8_t* buf = (uint8_t*)canvas.getBuffer();

    // [1] 전체를 1픽셀 왼쪽으로 이동 (각 행의 바이트를 왼쪽으로 1비트 시프트)
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        uint8_t* row = buf + y * stride;
        for (int i = 0; i < stride - 1; i++) row[i] = (uint8_t)((row[i] << 1) | (row[i + 1] >> 7));
        row[stride - 1] <<= 1;
    }

    // [2] 맨 오른쪽 열: 행마다 담당 밴드들의 최대 세기를 디더링으로 점 찍기 (맨 윗줄 = 최고 주파수)
    const uint8_t lastMask = 0x01;                 // x = CANVAS_WIDTH-1 의 비트
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        int b0 = (CANVAS_HEIGHT - 1 - y) * NUM_BANDS / CANVAS_HEIGHT;
        int b1 = (CANVAS_HEIGHT - y) * NUM_BANDS / CANVAS_HEIGHT;
        int level = 0;
        for (int b = b0; b < b1; b++) if (band_level[b] > level) level = band_level[b];

        // 세기 0~64 → 0~16, 문턱(0~15)보다 크면 점을 켠다
        if ((level >> 2) > BAYER4[y & 3][spectro_col & 3]) buf[y * stride + stride - 1] |= lastMask;
    }
    spectro_col++;

    last_render_time = micros() - start_time;
}

int AudioAnalyzer::bassLevel() const {
    int bass = 0;
    int nb = min((int)BASS_BANDS, (int)NUM_BANDS);
    for (int i = 0; i < nb; i++) if (band_level[i] > bass) bass = band_level[i];
    return bass;
}

/**
 * @brief 박자 판정 — 저역 세기가 이동 평균의 BEAT_RATIO배를 넘고 BEAT_MIN_LEVEL 이상이며 쿨다운이 끝났을 때 true
 * @details 호출할 때마다 이동 평균과 쿨다운을 갱신하므로 프레임당 한 번만 호출한다.
 */
bool AudioAnalyzer::detectBeat(int bass) {
    bool beat = false;
    if (beat_cooldown > 0) beat_cooldown--;
    if (beat_cooldown == 0 && bass >= BEAT_MIN_LEVEL && bass > bass_avg * BEAT_RATIO) {
        beat = true;
        beat_cooldown = BEAT_COOLDOWN;
    }
    bass_avg = bass_avg * 0.95f + bass * 0.05f;
    return beat;
}

/**
 * @brief 파티클 — 스펙트럼 막대가 높은 밴드의 x 위치(왼쪽 저음 → 오른쪽 고음) 위쪽에서 점이 떨어지고, 바닥에서 튕기다 사라진다
 * @details 스펙트럼 모드와 같은 막대 세기(AGC 정규화)를 기준으로 하므로 막대가 솟은 위치에서만, 세기에 비례해 계속 떨어진다.
 *          세기가 클수록 처음 속도가 빠르다. 바닥에서 튕길 때마다 가로 속도가 무작위로 바뀌어 옆으로도 튕긴다. 떨어지는 동안 속도에 비례한 꼬리를 그린다.
 *          생성 검사 시작 밴드를 무작위로 돌려 한쪽 주파수로 쏠리지 않게 한다.
 */
void AudioAnalyzer::renderParticles(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();

    canvas.clear();
    updateBands();

    // [1] 생성: 스펙트럼 막대(band_level)가 PARTICLE_MIN_LEVEL 이상인 밴드에서 세기에 비례한 확률로 지속 생성
    const int range = NUM_BANDS - PARTICLE_FIRST_BAND;
    const int band_width = CANVAS_WIDTH / NUM_BANDS;
    int spawned = 0;
    if (range > 0) {
        int startOff = random(range);
        for (int i = 0; i < range && spawned < PARTICLE_SPAWN_PER_FRAME; i++) {
            int b = PARTICLE_FIRST_BAND + (startOff + i) % range;
            int lv = band_level[b];
            if (lv < PARTICLE_MIN_LEVEL) continue;

            // 확률(0.01%) = RATE_PCT × 세기 비율 — 세기를 MIN_LEVEL~최대 구간에서 0~1로 정규화
            long norm = (long)(lv - PARTICLE_MIN_LEVEL) * 1000 / (CANVAS_HEIGHT - PARTICLE_MIN_LEVEL);   // 0~1000
            long chance = norm * PARTICLE_RATE_PCT / 10;                                                // 0~PARTICLE_RATE_PCT*100
            if (random(10000) >= chance) continue;

            int vy = 24 + lv * 104 / CANVAS_HEIGHT;           // 세기가 클수록 처음 속도가 빠르다 (약 1.5~8px/프레임)
            for (int p = 0; p < PARTICLE_MAX; p++) {
                if (particles[p].active) continue;
                particles[p] = { (int16_t)((b * band_width + band_width / 2) << 4), 0,
                                 (int16_t)random(-PARTICLE_SPAWN_VX, PARTICLE_SPAWN_VX + 1), (int16_t)vy, true };
                spawned++;
                break;
            }
        }
    }

    // [2] 이동·바닥 반사·그리기
    const int floorY = CANVAS_HEIGHT - 2;                 // 2x2 점이 바닥에 닿는 y
    for (int p = 0; p < PARTICLE_MAX; p++) {
        Particle& pt = particles[p];
        if (!pt.active) continue;

        pt.vy += PARTICLE_GRAVITY;
        pt.y += pt.vy;
        pt.x += pt.vx;
        if (pt.y >= (floorY << 4)) {
            pt.y = floorY << 4;
            pt.vy = -(pt.vy * PARTICLE_BOUNCE_PCT / 100);
            if (-pt.vy < PARTICLE_MIN_BOUNCE) { pt.active = false; continue; }
            // 튕길 때마다 옆으로도 무작위로 튀게 한다
            pt.vx = constrain(pt.vx + (int)random(-PARTICLE_BOUNCE_VX, PARTICLE_BOUNCE_VX + 1), -PARTICLE_MAX_VX, PARTICLE_MAX_VX);
        }
        if (pt.x < 0 || pt.x >= (CANVAS_WIDTH << 4)) { pt.active = false; continue; }   // 화면 밖

        int px = pt.x >> 4;
        int y = pt.y >> 4;
        canvas.fillRect(px - 1, y, 2, 2, COLOR_WHITE);
        if (pt.vy > 0) {                                  // 낙하 중에만 진행 반대 방향 꼬리
            int trail = min(6, (int)(pt.vy >> 4));
            if (trail > 0) canvas.drawLine(px - 1 - (int)((long)pt.vx * trail / pt.vy), y - trail, px - 1, y, COLOR_WHITE);
        }
    }

    last_render_time = micros() - start_time;
}

/**
 * @brief 도트 매트릭스 — 밴드마다 DOT_PITCH 간격의 점 기둥, 꺼진 점은 가운데 1픽셀만 희미하게 표시
 * @details 켜진 점 개수 = 막대 높이/DOT_PITCH(올림). 피크는 점 하나로 표시하고,
 *          박자가 잡히면 DOT_FLASH_FRAMES 프레임 동안 화면 전체를 반전시킨다.
 */
void AudioAnalyzer::renderDotMatrix(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();

    canvas.clear();
    updateBands();

    if (detectBeat(bassLevel())) flash_left = DOT_FLASH_FRAMES;

    const int rows = CANVAS_HEIGHT / DOT_PITCH;
    const int cols = CANVAS_WIDTH / DOT_PITCH;
    const int off = (DOT_PITCH - DOT_SIZE + 1) / 2;   // 점을 칸 안에 가운데 정렬
    for (int c = 0; c < cols && c < NUM_BANDS; c++) {
        int x = c * DOT_PITCH + off;
        int lit = (smooth_bar[c] + DOT_PITCH - 1) / DOT_PITCH;     // 켜진 점 수
        int peakRow = (peak[c] + DOT_PITCH - 1) / DOT_PITCH;       // 피크 점 위치(1~rows, 0이면 없음)
        for (int r = 0; r < rows; r++) {
            int y = CANVAS_HEIGHT - (r + 1) * DOT_PITCH + off;
            if (r < lit || r == peakRow - 1) canvas.fillRect(x, y, DOT_SIZE, DOT_SIZE, COLOR_WHITE);
            else canvas.drawPixel(x + DOT_SIZE / 2, y + DOT_SIZE / 2, COLOR_WHITE);
        }
    }

    // 박자 플래시: 1비트 버퍼 전체 반전
    if (flash_left > 0) {
        flash_left--;
        uint8_t* buf = (uint8_t*)canvas.getBuffer();
        for (int i = 0; i < CANVAS_WIDTH / 8 * CANVAS_HEIGHT; i++) buf[i] = ~buf[i];
    }

    last_render_time = micros() - start_time;
}

/**
 * @brief 미러 스펙트럼 — 중앙선(y=CANVAS_HEIGHT/2) 기준 위아래 대칭 막대와 피크 도트
 * @details 막대 높이(0~CANVAS_HEIGHT)를 절반으로 나눠 위·아래로 각각 그린다.
 */
void AudioAnalyzer::renderMirror(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();

    canvas.clear();
    updateBands();

    const int centerY = CANVAS_HEIGHT / 2;
    int band_width = CANVAS_WIDTH / NUM_BANDS;
    for (int i = 0; i < NUM_BANDS; i++) {
        int x = i * band_width;

        // 중앙선 위·아래로 half 픽셀씩 (총 높이 2*half)
        int half = (smooth_bar[i] + 1) / 2;
        if (half > 0) canvas.fillRect(x, centerY - half, band_width - 1, half * 2, COLOR_WHITE);

        // 피크 도트: 위쪽 도트와 중앙선 기준 대칭 위치에 아래쪽 도트
        int peak_y = centerY - (peak[i] + 1) / 2 - 1;
        if (peak_y < 0) peak_y = 0; else if (peak_y > centerY - PEAK_DOT_HEIGHT) peak_y = centerY - PEAK_DOT_HEIGHT;
        canvas.fillRect(x, peak_y, band_width - 1, PEAK_DOT_HEIGHT, COLOR_WHITE);
        canvas.fillRect(x, CANVAS_HEIGHT - peak_y - PEAK_DOT_HEIGHT, band_width - 1, PEAK_DOT_HEIGHT, COLOR_WHITE);
    }

    last_render_time = micros() - start_time;
}
float AudioAnalyzer::getBandAmplitude(int band_idx) {
    if (band_idx < 0 || band_idx >= NUM_BANDS) return 0;

    float amp = 0;
    float startBin = band_mappings[band_idx].startBin;
    float endBin = band_mappings[band_idx].endBin;

    // [1] 선형 모드(MAPPING_MODE == 0) - 고속 1:1 매핑
    if (MAPPING_MODE == 0) {
        int idx = (int)startBin;
        if (idx < (SAMPLES / 2)) {
            amp = vReal[idx];
        }
    } 
    // [2] 담당 Bin 너비가 1 이하인 경우 (주로 저음 대역) - 선형 보간
    else if (endBin - startBin <= 1.0f) {
        int idx = (int)startBin;
        float fract = startBin - idx;
        if (idx < (SAMPLES / 2) - 1) {
            amp = vReal[idx] * (1.0f - fract) + vReal[idx + 1] * fract;
        } else {
            amp = vReal[idx];
        }
    } 
    // [3] 담당 Bin 너비가 1보다 큰 경우 (주로 고음 대역) - 에너지 합산(Sum Pooling)
    else {
        int sIdx = (int)floor(startBin);
        int eIdx = (int)ceil(endBin);
        for (int b = sIdx; b < eIdx && b < (SAMPLES / 2); b++) {
            amp += vReal[b];
        }
    }
    return amp;
}

// 1/4 전송 기능은 병합된 render로 대체됨 (삭제)

void AudioAnalyzer::renderWaveform(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();
    canvas.clear();

    int centerY = CANVAS_HEIGHT / 2;

    // [1] 중앙 기준선 (0V) — 가로 점선
    for (int x = 0; x < CANVAS_WIDTH; x += 2) {
        canvas.drawPixel(x, centerY, COLOR_WHITE);
    }

    // [3] 파형 자동 스케일링 (AGC)
    float maxAmp = 1.0f;
    for (int i = 0; i < SAMPLES; i++) {
        float absVal = fabs(waveformBuf[i]);
        if (absVal > maxAmp) maxAmp = absVal;
    }
    float scale = (float)(centerY - 2) / maxAmp; // 상하 2px 여유

    // [4] 파형 그리기 (1024 샘플 → 512px 다운샘플링, 라인 연결)
    int prevIdx = 0;
    int prevY = centerY - (int)(waveformBuf[0] * scale);
    prevY = constrain(prevY, 0, CANVAS_HEIGHT - 1);

    for (int x = 1; x < CANVAS_WIDTH; x++) {
        int idx = x * SAMPLES / CANVAS_WIDTH;
        int curY = centerY - (int)(waveformBuf[idx] * scale);
        curY = constrain(curY, 0, CANVAS_HEIGHT - 1);
        canvas.drawLine(x - 1, prevY, x, curY, COLOR_WHITE);
        prevY = curY;
    }

    last_render_time = micros() - start_time;
}
