// worynim@gmail.com
/**
 * @file stream_scanner.h
 * @brief HTML 응답에서 키 바로 뒤의 값을 꺼내는 고정 메모리 스캐너
 * @details 응답을 조각 단위로 넣으면 최근 kWindow 바이트만 기억하며 키를 찾는다.
 *          줄을 통째로 String에 올리지 않으므로 수십 KB~수 MB짜리 한 줄(네이버·YouTube)도
 *          힙을 더 쓰지 않는다. Arduino에 의존하지 않아 PC에서도 시험할 수 있다.
 */
#ifndef STREAM_SCANNER_H
#define STREAM_SCANNER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** 찾을 키 하나 — key 바로 뒤의 값을 최대 max_len 바이트, stop 문자 전까지 꺼낸다 */
struct ScanKey {
  const char *key;
  uint8_t max_len;   // 값 최대 길이 (StreamScanner::kMaxValue 이하로 잘린다)
  char stop;         // 이 문자가 나오면 값 끝 (0이면 max_len까지)
};

class StreamScanner {
public:
  static const size_t kWindow = 96;    // 가장 긴 키(바이트)보다 커야 한다
  static const size_t kMaxValue = 32;
  static const int kMaxKeys = 4;

  StreamScanner(const ScanKey *keys, int count) : keys_(keys), count_(count < kMaxKeys ? count : kMaxKeys) {
    for (int k = 0; k < count_; k++) key_len_[k] = strlen(keys_[k].key);
  }

  /**
   * 조각을 넣는다. 값이 완성될 때마다 onValue(키 번호, 값)를 부른다.
   * @return onValue가 false를 돌려줘 멈췄으면 false
   */
  template <typename F>
  bool feed(const uint8_t *data, size_t len, F onValue) {
    for (size_t i = 0; i < len; i++) {
      char c = (char)data[i];

      // 값 수집 중: stop 문자나 최대 길이에서 끝낸다
      if (active_ >= 0) {
        const ScanKey &k = keys_[active_];
        size_t max_len = k.max_len < kMaxValue ? k.max_len : kMaxValue;
        if (k.stop != 0 && c == k.stop) {
          if (!finish(onValue)) return false;
          continue;
        }
        value_[vlen_++] = c;
        if (vlen_ >= max_len && !finish(onValue)) return false;
        continue;
      }

      ring_[pos_] = c;
      pos_ = (pos_ + 1) % kWindow;
      if (filled_ < kWindow) filled_++;

      for (int k = 0; k < count_; k++) {
        if (endsWith(k, c)) {
          active_ = k;
          vlen_ = 0;
          break;
        }
      }
    }
    return true;
  }

private:
  /** 방금 넣은 바이트 c로 끝나는 최근 바이트들이 키 k와 같은가 */
  bool endsWith(int k, char c) const {
    size_t n = key_len_[k];
    if (n == 0 || n > filled_ || keys_[k].key[n - 1] != c) return false;
    for (size_t j = 1; j < n; j++) {
      size_t idx = (pos_ + kWindow - 1 - j) % kWindow;
      if (ring_[idx] != keys_[k].key[n - 1 - j]) return false;
    }
    return true;
  }

  template <typename F>
  bool finish(F onValue) {
    value_[vlen_] = '\0';
    int k = active_;
    active_ = -1;
    return onValue(k, (const char *)value_);
  }

  const ScanKey *keys_;
  int count_;
  size_t key_len_[kMaxKeys] = {0};
  char ring_[kWindow] = {0};
  size_t pos_ = 0;
  size_t filled_ = 0;
  int active_ = -1;              // 값을 모으는 중인 키 번호 (-1: 키 찾는 중)
  char value_[kMaxValue + 1] = {0};
  size_t vlen_ = 0;
};

#endif
