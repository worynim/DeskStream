/**
 * Chiptune_Buzzer — DeskStream(ESP32-C3) 부저 칩튠 테스트
 *
 * 원본: Linus Åkesson "Hardware Chiptune" 플레이어, Stimmer의 Arduino 포트
 *       (Instructables "Arduino Chiptunes")를 ESP32-C3로 옮긴 테스트 스케치.
 *
 * 구조
 *  - 하드웨어 타이머 ISR (약 15.9kHz): 4채널 오실레이터 합성 → LEDC duty(GPIO 7) 갱신
 *  - loop() (약 88Hz 틱): 곡/트랙/악기 해석(playroutine)
 *
 * 버튼
 *  - BTN1 (GPIO 1): 처음부터 재생
 *  - BTN2 (GPIO 4): 정지
 *
 * AVR 대비 변경점
 *  - Timer0/Timer2 레지스터 → gptimer(timerBegin) + LEDC PWM (78.125kHz, 8비트)
 *  - s8 를 char → int8_t 로 변경 (RISC-V에서 char는 unsigned)
 *  - ISR↔loop 틱 전달을 증가 카운터 방식으로 변경 (원본의 RMW 경합 제거)
 *  - 곡이 끝나거나 정지하면 PWM duty 0 (부저 코일에 50% duty가 계속 걸리지 않도록)
 */

#include <Arduino.h>
#include "hal/ledc_ll.h"
#include "song_data.h"

// ===================== 설정 =====================
#define BUZZER_PIN      7
#define BTN_PLAY_PIN    1       // BTN1
#define BTN_STOP_PIN    4       // BTN2

#define PWM_CHANNEL     0       // LEDC 채널 (ISR에서 레지스터 직접 갱신)
#define PWM_FREQ        78125   // 80MHz / 256 / 4 → 가청 대역 밖 캐리어
#define PWM_BITS        8

#define TIMER_HZ        1000000 // 타이머 1MHz (1틱 = 1us)
#define SAMPLE_PERIOD   63      // 1e6/63 ≈ 15873Hz (원본 Arduino 16MHz 포트와 동일 샘플레이트)
#define TICK_SAMPLES    180     // 180샘플마다 playroutine 1회 (≈ 88Hz)

// ===================== 타입 =====================
typedef uint8_t  u8;
typedef uint16_t u16;
typedef int8_t   s8;    // 중요: RISC-V의 char는 unsigned
typedef int16_t  s16;
typedef uint32_t u32;

// Arduino 자동 전방선언보다 먼저 보이도록 상단에 둠
struct Button {
  u8 pin;
  bool last;
  u32 changedAt;
};

// ===================== 테이블 =====================
// 16kHz 샘플레이트 기준 음정 테이블 (원본 8kHz 테이블을 한 옥타브 내린 값)
static const u16 freqtable[] = {
  0x0085, 0x008d, 0x0096, 0x009f, 0x00a8, 0x00b2, 0x00bd, 0x00c8, 0x00d4,
  0x00e1, 0x00ee, 0x00fc, 0x010b, 0x011b, 0x012c, 0x013e, 0x0151, 0x0165,
  0x017a, 0x0191, 0x01a9, 0x01c2, 0x01dd, 0x01f9, 0x0217, 0x0237, 0x0259,
  0x027d, 0x02a3, 0x02cb, 0x02f5, 0x0322, 0x0352, 0x0385, 0x03ba, 0x03f3,
  0x042f, 0x046f, 0x04b2, 0x04fa, 0x0546, 0x0596, 0x05eb, 0x0645, 0x06a5,
  0x070a, 0x0775, 0x07e6, 0x085f, 0x08de, 0x0965, 0x09f4, 0x0a8c, 0x0b2c,
  0x0bd6, 0x0c8b, 0x0d4a, 0x0e14, 0x0eea, 0x0fcd, 0x10be, 0x11bd, 0x12cb,
  0x13e9, 0x1518, 0x1659, 0x17ad, 0x1916, 0x1a94, 0x1c28, 0x1dd5, 0x1f9b,
  0x217c, 0x237a, 0x2596, 0x27d3, 0x2a31, 0x2cb3, 0x2f5b, 0x322c, 0x3528,
  0x3851, 0x3bab, 0x3f37
};
#define FREQTABLE_LEN (sizeof(freqtable) / sizeof(freqtable[0]))

static const s8 sinetable[] = {
  0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126,
  127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12,
  0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122,
  -125, -126, -127, -126, -125, -122, -117, -112, -106, -98, -90, -81,
  -71, -60, -49, -37, -25, -12
};

static const char validcmds[] = "0dfijlmtvw~+=";

enum { WF_TRI, WF_SAW, WF_PUL, WF_NOI };

// ===================== 상태 =====================
struct oscillator {
  u16 freq;
  u16 phase;
  u16 duty;
  u8  waveform;
  u8  volume;   // 0-255
};
static volatile oscillator osc[4];

struct unpacker {
  u16 nextbyte;
  u8  buffer;
  u8  bits;
};

struct channel_t {
  unpacker trackup;
  u8  tnum;
  s8  transp;
  u8  tnote;
  u8  lastinstr;
  u8  inum;
  u16 iptr;
  u8  iwait;
  u8  inote;
  s8  bendd;
  s16 bend;
  s8  volumed;
  s16 dutyd;
  u8  vdepth;
  u8  vrate;
  u8  vpos;
  s16 inertia;
  u16 slur;
};
static channel_t channel[4];

static u16 resources[16 + MAXTRACK];
static unpacker songup;

static u8 trackwait, trackpos, playsong, songpos;

// ISR 전용
static u32 noiseseed = 1;
static u8  lastsample = 128;
static u8  callbackwait = 0;

// ISR → loop
static volatile u32  tickCount = 0;   // ISR이 증가만 함
static volatile bool audioOn = false; // false면 PWM duty 0
static u32 ticksDone = 0;
static u32 endAt = 0;                 // 곡 종료 시각 (0 = 대기 안 함)

static hw_timer_t *sampleTimer = nullptr;

// ===================== LEDC 직접 갱신 (ISR용) =====================
static inline void IRAM_ATTR pwmWriteFast(u8 duty) {
  ledc_ll_set_duty_int_part(&LEDC, LEDC_LOW_SPEED_MODE, (ledc_channel_t)PWM_CHANNEL, duty);
  ledc_ll_set_duty_start(&LEDC, LEDC_LOW_SPEED_MODE, (ledc_channel_t)PWM_CHANNEL);
  ledc_ll_ls_channel_update(&LEDC, LEDC_LOW_SPEED_MODE, (ledc_channel_t)PWM_CHANNEL);
}

// ===================== 사운드 ISR =====================
void ARDUINO_ISR_ATTR onSample() {
  if (!audioOn) {
    pwmWriteFast(0);
    return;
  }

  pwmWriteFast(lastsample);

  // 32비트 LFSR 노이즈
  u8 newbit = 0;
  if (noiseseed & 0x80000000UL) newbit ^= 1;
  if (noiseseed & 0x01000000UL) newbit ^= 1;
  if (noiseseed & 0x00000040UL) newbit ^= 1;
  if (noiseseed & 0x00000200UL) newbit ^= 1;
  noiseseed = (noiseseed << 1) | newbit;

  if (callbackwait) {
    callbackwait--;
  } else {
    tickCount = tickCount + 1;
    callbackwait = TICK_SAMPLES - 1;
  }

  s16 acc = 0;
  for (u8 i = 0; i < 4; i++) {
    s8 value;  // [-32, 31]
    u16 phase = osc[i].phase;
    switch (osc[i].waveform) {
      case WF_TRI:
        if (phase < 0x8000) value = -32 + (phase >> 9);
        else                value = 31 - ((phase - 0x8000) >> 9);
        break;
      case WF_SAW:
        value = -32 + (phase >> 10);
        break;
      case WF_PUL:
        value = (phase > osc[i].duty) ? -32 : 31;
        break;
      case WF_NOI:
        value = (s8)((noiseseed & 63) - 32);
        break;
      default:
        value = 0;
        break;
    }
    osc[i].phase = phase + osc[i].freq;
    acc += value * osc[i].volume;   // [-8160, 7905]
  }
  lastsample = 128 + (acc >> 8);    // [1, 251]
}

// ===================== 곡 데이터 해석 =====================
static inline u8 readsongbyte(u16 offset) {
  return pgm_read_byte(&songdata[offset]);
}

static void initup(unpacker *up, u16 offset) {
  up->nextbyte = offset;
  up->bits = 0;
}

static u8 readbit(unpacker *up) {
  if (!up->bits) {
    up->buffer = readsongbyte(up->nextbyte++);
    up->bits = 8;
  }
  up->bits--;
  u8 val = up->buffer & 1;
  up->buffer >>= 1;
  return val;
}

static u16 readchunk(unpacker *up, u8 n) {
  u16 val = 0;
  for (u8 i = 0; i < n; i++) {
    if (readbit(up)) val |= (1 << i);
  }
  return val;
}

static void readinstr(u8 num, u16 pos, u8 *dest) {
  dest[0] = readsongbyte(resources[num] + 2 * pos + 0);
  dest[1] = readsongbyte(resources[num] + 2 * pos + 1);
}

static void runcmd(u8 ch, u8 cmd, u8 param) {
  if (cmd >= sizeof(validcmds) - 1) return;
  switch (validcmds[cmd]) {
    case '0': channel[ch].inum = 0; break;
    case 'd': osc[ch].duty = param << 8; break;
    case 'f': channel[ch].volumed = (s8)param; break;
    case 'i': channel[ch].inertia = param << 1; break;
    case 'j': channel[ch].iptr = param; break;
    case 'l': channel[ch].bendd = (s8)param; break;
    case 'm': channel[ch].dutyd = param << 6; break;
    case 't': channel[ch].iwait = param; break;
    case 'v': osc[ch].volume = param; break;
    case 'w': osc[ch].waveform = param; break;
    case '+': channel[ch].inote = param + channel[ch].tnote - 12 * 4; break;
    case '=': channel[ch].inote = param; break;
    case '~':
      if (channel[ch].vdepth != (param >> 4)) channel[ch].vpos = 0;
      channel[ch].vdepth = param >> 4;
      channel[ch].vrate = param & 15;
      break;
  }
}

static void initresources() {
  unpacker up;
  initup(&up, 0);
  for (u16 i = 0; i < 16 + MAXTRACK; i++) {
    resources[i] = readchunk(&up, 13);
  }
  initup(&songup, resources[0]);
}

// 틱마다 1회 (≈ 88Hz)
static void playroutine() {
  if (playsong) {
    if (trackwait) {
      trackwait--;
    } else {
      trackwait = 4;   // 5틱마다 한 줄

      if (!trackpos) {
        if (songpos >= SONGLEN) {
          playsong = 0;
        } else {
          for (u8 ch = 0; ch < 4; ch++) {
            u8 gottransp = readchunk(&songup, 1);
            channel[ch].tnum = readchunk(&songup, 6);
            u8 transp = 0;
            if (gottransp) {
              transp = readchunk(&songup, 4);
              if (transp & 0x8) transp |= 0xf0;   // 4비트 부호 확장
            }
            channel[ch].transp = (s8)transp;
            if (channel[ch].tnum) {
              initup(&channel[ch].trackup, resources[16 + channel[ch].tnum - 1]);
            }
          }
          songpos++;
        }
      }

      if (playsong) {
        for (u8 ch = 0; ch < 4; ch++) {
          if (!channel[ch].tnum) continue;

          u8 fields = readchunk(&channel[ch].trackup, 3);
          u8 note = 0, instr = 0, cmd = 0, param = 0;
          if (fields & 1) note  = readchunk(&channel[ch].trackup, 7);
          if (fields & 2) instr = readchunk(&channel[ch].trackup, 4);
          if (fields & 4) {
            cmd   = readchunk(&channel[ch].trackup, 4);
            param = readchunk(&channel[ch].trackup, 8);
          }
          if (note) {
            channel[ch].tnote = note + channel[ch].transp;
            if (!instr) instr = channel[ch].lastinstr;
          }
          if (instr) {
            channel[ch].lastinstr = instr;
            channel[ch].inum = instr;
            channel[ch].iptr = 0;
            channel[ch].iwait = 0;
            channel[ch].bend = 0;
            channel[ch].bendd = 0;
            channel[ch].volumed = 0;
            channel[ch].dutyd = 0;
            channel[ch].vdepth = 0;
          }
          if (cmd) runcmd(ch, cmd, param);
        }
        trackpos = (trackpos + 1) & 31;
      }
    }
  }

  for (u8 ch = 0; ch < 4; ch++) {
    // 악기 스크립트: 't'(대기)가 나올 때까지 실행
    u8 guard = 0;
    while (channel[ch].inum && !channel[ch].iwait && guard++ < 64) {
      u8 il[2];
      readinstr(channel[ch].inum, channel[ch].iptr, il);
      channel[ch].iptr++;
      runcmd(ch, il[0], il[1]);
    }
    if (channel[ch].iwait) channel[ch].iwait--;

    u8 inote = channel[ch].inote;
    if (inote >= FREQTABLE_LEN) inote = FREQTABLE_LEN - 1;

    u16 slur;
    if (channel[ch].inertia) {
      slur = channel[ch].slur;
      s16 diff = freqtable[inote] - slur;
      if (diff > channel[ch].inertia)       diff = channel[ch].inertia;
      else if (diff < -channel[ch].inertia) diff = -channel[ch].inertia;
      slur += diff;
      channel[ch].slur = slur;
    } else {
      slur = freqtable[inote];
    }

    osc[ch].freq = slur + channel[ch].bend +
                   ((channel[ch].vdepth * sinetable[channel[ch].vpos & 63]) >> 2);
    channel[ch].bend += channel[ch].bendd;

    s16 vol = osc[ch].volume + channel[ch].volumed;
    if (vol < 0)   vol = 0;
    if (vol > 255) vol = 255;
    osc[ch].volume = vol;

    u16 duty = osc[ch].duty + channel[ch].dutyd;
    if (duty > 0xe000) duty = 0x2000;
    if (duty < 0x2000) duty = 0xe000;
    osc[ch].duty = duty;

    channel[ch].vpos += channel[ch].vrate;
  }
}

// ===================== 재생 제어 =====================
static void stopSong() {
  audioOn = false;
  playsong = 0;
  for (u8 ch = 0; ch < 4; ch++) osc[ch].volume = 0;
}

static void startSong() {
  audioOn = false;   // ISR이 오실레이터를 읽지 않게 한 뒤 초기화

  memset(channel, 0, sizeof(channel));
  for (u8 ch = 0; ch < 4; ch++) {
    osc[ch].freq = 0;
    osc[ch].phase = 0;
    osc[ch].duty = 0x8000;
    osc[ch].waveform = WF_TRI;
    osc[ch].volume = 0;
  }
  trackwait = 0;
  trackpos = 0;
  songpos = 0;
  playsong = 1;
  initresources();

  ticksDone = tickCount;
  endAt = 0;
  audioOn = true;
  Serial.println("[Chiptune] play");
}

// ===================== 버튼 (하강 에지 + 디바운스) =====================
static Button btnPlay = {BTN_PLAY_PIN, true, 0};
static Button btnStop = {BTN_STOP_PIN, true, 0};

static bool pressed(Button &b) {
  bool now = digitalRead(b.pin);
  u32 t = millis();
  if (now != b.last && t - b.changedAt > 30) {
    b.last = now;
    b.changedAt = t;
    return now == LOW;
  }
  return false;
}

// ===================== Arduino =====================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_PLAY_PIN, INPUT_PULLUP);
  pinMode(BTN_STOP_PIN, INPUT_PULLUP);

  if (!ledcAttachChannel(BUZZER_PIN, PWM_FREQ, PWM_BITS, PWM_CHANNEL)) {
    Serial.println("[Chiptune] LEDC attach failed");
  }
  ledcWrite(BUZZER_PIN, 0);   // duty 관련 레지스터(방향/스텝 등) 초기화

  sampleTimer = timerBegin(TIMER_HZ);
  timerAttachInterrupt(sampleTimer, &onSample);
  timerAlarm(sampleTimer, SAMPLE_PERIOD, true, 0);

  Serial.println("[Chiptune] BTN1=play, BTN2=stop");
  startSong();
}

void loop() {
  if (pressed(btnPlay)) startSong();
  if (pressed(btnStop)) {
    stopSong();
    Serial.println("[Chiptune] stop");
  }

  // ISR이 쌓은 틱만큼 playroutine 실행 (밀린 틱은 최대 4개까지만 따라잡음)
  u32 pending = tickCount - ticksDone;
  if (pending > 4) {
    ticksDone = tickCount - 4;
    pending = 4;
  }
  while (pending--) {
    ticksDone++;
    playroutine();
  }

  if (audioOn && !playsong) {
    // 곡 종료: 마지막 음이 잦아들 시간을 조금 준 뒤 출력 끔
    if (!endAt) endAt = millis();
    if (millis() - endAt > 1500) {
      stopSong();
      endAt = 0;
      Serial.println("[Chiptune] end");
    }
  }
}
