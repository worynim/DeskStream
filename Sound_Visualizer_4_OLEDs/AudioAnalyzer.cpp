#include "AudioAnalyzer.h"
#include "soc/soc_caps.h"

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
    noise_floor = new float[numBands];
    band_mappings = new BandMapping[numBands]; // 매핑 테이블 메모리 할당
    
    // ADC DMA 결과 버퍼 할당 (SAMPLES개 × 4바이트)
    adc_raw_buf = new uint8_t[samples * SOC_ADC_DIGI_RESULT_BYTES];
    
    // 초기화 (사용자가 설정한 바 개수만큼)
    for (int i = 0; i < numBands; i++) {
        peak[i] = 0;
        peak_hold[i] = 0;
        smooth_bar[i] = 0;
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
    delete[] noise_floor;
    delete[] band_mappings;
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
    dsps_fft2r_init_sc16(NULL, 1024);
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
    
    extern int DEBUG_MODE;
    if (DEBUG_MODE >= 3) {
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

void AudioAnalyzer::render(LGFX_Sprite& canvas) {
    uint32_t start_time = micros();

    canvas.clear();

    uint16_t num_bands = NUM_BANDS;
    int band_width = CANVAS_WIDTH / num_bands; 

    // AGC (Auto Gain Control) 로직
    float current_frame_max = 0;
    for (int i = 0; i < num_bands; i++) {
        if (vReal[i] > current_frame_max) current_frame_max = vReal[i];
    }
    
    if (current_frame_max > agc_max_amp) agc_max_amp = current_frame_max;
    else agc_max_amp = (agc_max_amp * AGC_DECAY_RATE) + (current_frame_max * (1.0f - AGC_DECAY_RATE));
    
    if (agc_max_amp < AGC_MIN_AMPLITUDE) agc_max_amp = AGC_MIN_AMPLITUDE;

    // 밴드 렌더링
    for (int i = 0; i < num_bands; i++) {
        float amp = getBandAmplitude(i);

        float threshold = noise_floor[i];
        if (amp < threshold) amp = 0; else amp -= threshold;

        int target_height = map((long)amp, 0, (long)agc_max_amp , 0, CANVAS_HEIGHT);
        target_height = constrain(target_height, 0, CANVAS_HEIGHT);

        if (target_height > smooth_bar[i]) smooth_bar[i] = target_height;
        else {
            if (smooth_bar[i] > 0) smooth_bar[i] -= BAR_DROP_SPEED;
            if (smooth_bar[i] < 0) smooth_bar[i] = 0;
        }
        int bar_height = smooth_bar[i];

        int x = i * band_width;
        int y = CANVAS_HEIGHT - bar_height;
        if (bar_height > 0) canvas.fillRect(x, y, band_width - 1, bar_height, COLOR_WHITE);

        // 피크(Peak) 도트 렌더링
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

        int peak_y = CANVAS_HEIGHT - peak[i] - 1;
        if (peak_y < 0) peak_y = 0; else if (peak_y > (CANVAS_HEIGHT - 2)) peak_y = (CANVAS_HEIGHT - 2);
        canvas.fillRect(x, peak_y, band_width - 1, PEAK_DOT_HEIGHT, COLOR_WHITE);
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
