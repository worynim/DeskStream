# worynim@gmail.com
"""
@file image_processor.py
@brief 흑백 이진화 및 디더링 처리 유틸리티 (screen_streamer.py에서 분리)
@details apply_threshold, apply_dithering, pack_bits 및 Numba JIT 커널 포함
"""

import numpy as np
from PIL import Image, ImageEnhance

try:
    from numba import njit
    HAS_NUMBA = True
except ImportError:
    HAS_NUMBA = False
    def njit(*args, **kwargs):
        def decorator(func):
            return func
        if len(args) == 1 and callable(args[0]):
            return args[0]
        return decorator
# =============================================
# [이미지 처리 유틸리티]
# =============================================

def apply_threshold(img_gray: Image.Image, threshold: int) -> np.ndarray:
    """
    단순 임계값 이진화.

    Parameters:
        img_gray  : 8-bit grayscale PIL Image (512×64)
        threshold : 0~255. 이 값 이상이면 흰색, 미만이면 검정.

    Returns:
        np.ndarray (shape: (64, 512), dtype: uint8), 0=검정 255=흰색
    """
    arr = np.array(img_gray, dtype=np.uint8)
    return (arr >= threshold).astype(np.uint8) * 255

@njit(cache=True)
def _dither_fs(arr: np.ndarray, h: int, w: int):
    for y in range(h):
        for x in range(w):
            old = arr[y, x]
            new = 255.0 if old >= 128.0 else 0.0
            arr[y, x] = new
            err = old - new
            if x + 1 < w: arr[y, x + 1] += err * (7.0/16.0)
            if y + 1 < h:
                if x - 1 >= 0: arr[y + 1, x - 1] += err * (3.0/16.0)
                arr[y + 1, x] += err * (5.0/16.0)
                if x + 1 < w: arr[y + 1, x + 1] += err * (1.0/16.0)

@njit(cache=True)
def _dither_atkinson(arr: np.ndarray, h: int, w: int):
    err_div = 1.0 / 8.0
    for y in range(h):
        for x in range(w):
            old = arr[y, x]
            new = 255.0 if old >= 128.0 else 0.0
            arr[y, x] = new
            err = (old - new) * err_div
            if x + 1 < w: arr[y, x + 1] += err
            if x + 2 < w: arr[y, x + 2] += err
            if y + 1 < h:
                if x - 1 >= 0: arr[y + 1, x - 1] += err
                arr[y + 1, x] += err
                if x + 1 < w: arr[y + 1, x + 1] += err
            if y + 2 < h: arr[y + 2, x] += err

@njit(cache=True)
def _dither_stucki(arr: np.ndarray, h: int, w: int):
    err_div = 1.0 / 42.0
    for y in range(h):
        for x in range(w):
            old = arr[y, x]
            new = 255.0 if old >= 128.0 else 0.0
            arr[y, x] = new
            err = (old - new) * err_div
            if x + 1 < w: arr[y, x + 1] += err * 8.0
            if x + 2 < w: arr[y, x + 2] += err * 4.0
            if y + 1 < h:
                if x - 2 >= 0: arr[y + 1, x - 2] += err * 2.0
                if x - 1 >= 0: arr[y + 1, x - 1] += err * 4.0
                arr[y + 1, x] += err * 8.0
                if x + 1 < w: arr[y + 1, x + 1] += err * 4.0
                if x + 2 < w: arr[y + 1, x + 2] += err * 2.0
            if y + 2 < h:
                if x - 2 >= 0: arr[y + 2, x - 2] += err * 1.0
                if x - 1 >= 0: arr[y + 2, x - 1] += err * 2.0
                arr[y + 2, x] += err * 4.0
                if x + 1 < w: arr[y + 2, x + 1] += err * 2.0
                if x + 2 < w: arr[y + 2, x + 2] += err * 1.0

@njit(cache=True)
def _dither_sierra(arr: np.ndarray, h: int, w: int):
    err_div = 1.0 / 32.0
    for y in range(h):
        for x in range(w):
            old = arr[y, x]
            new = 255.0 if old >= 128.0 else 0.0
            arr[y, x] = new
            err = (old - new) * err_div
            if x + 1 < w: arr[y, x + 1] += err * 5.0
            if x + 2 < w: arr[y, x + 2] += err * 3.0
            if y + 1 < h:
                if x - 2 >= 0: arr[y + 1, x - 2] += err * 2.0
                if x - 1 >= 0: arr[y + 1, x - 1] += err * 4.0
                arr[y + 1, x] += err * 5.0
                if x + 1 < w: arr[y + 1, x + 1] += err * 4.0
                if x + 2 < w: arr[y + 1, x + 2] += err * 2.0
            if y + 2 < h:
                if x - 1 >= 0: arr[y + 2, x - 1] += err * 2.0
                arr[y + 2, x] += err * 3.0
                if x + 1 < w: arr[y + 2, x + 1] += err * 2.0

def apply_dithering(img_gray: Image.Image, method: str = "Floyd-Steinberg", contrast: float = 1.0) -> np.ndarray:
    """
    다양한 디더링 알고리즘 적용.

    [성능 전략]
    - Numba JIT 있음: 각 알고리즘별 최적화된 JIT 함수 사용 (~1ms)
    - Numba 없음: PIL 내장 Floyd-Steinberg C 구현 사용 (~1ms)
      (순수 Python 루프 100+ms 대비 100배 빠름)
    """
    enhanced = ImageEnhance.Contrast(img_gray).enhance(contrast)

    # Ordered Bayer: numpy 벡터 연산 (항상 빠름, Numba 불필요)
    if method == "Ordered (Bayer)":
        arr = np.array(enhanced, dtype=np.float32)
        h, w = arr.shape
        bayer = np.array([
            [ 0,  8,  2, 10],
            [12,  4, 14,  6],
            [ 3, 11,  1,  9],
            [15,  7, 13,  5]
        ], dtype=np.float32)
        threshold_map = np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w]
        threshold_map = (threshold_map / 16.0) * 255.0
        return (arr >= threshold_map).astype(np.uint8) * 255

    # Numba JIT 가속 사용 가능 시: 각 알고리즘별 최적화된 커널
    if HAS_NUMBA:
        arr = np.array(enhanced, dtype=np.float32)
        h, w = arr.shape
        if method == "Floyd-Steinberg":
            _dither_fs(arr, h, w)
        elif method == "Atkinson":
            _dither_atkinson(arr, h, w)
        elif method == "Stucki":
            _dither_stucki(arr, h, w)
        elif method == "Sierra":
            _dither_sierra(arr, h, w)
        return np.clip(arr, 0.0, 255.0).astype(np.uint8)

    # Numba 없음: PIL 내장 Floyd-Steinberg 사용 (C 구현, ~1ms)
    # 모든 디더링 모드를 PIL 기반으로 대체하여 성능 확보
    # (순수 Python 루프 100~140ms → PIL C 구현 ~1ms)
    bw_pil = enhanced.convert('1')  # PIL 내장 Floyd-Steinberg (C 코드)
    return np.array(bw_pil.convert('L'))  # 0/255 uint8 배열로 변환


def pack_bits(mono_array: np.ndarray) -> bytes:
    """
    흑백 픽셀 배열(0/255, shape=(H, W))을 LovyanGFX 1-bit Sprite 포맷으로 패킹.

    [중요] ESP32 펌웨어의 LGFX_Sprite(1-bit) 메모리 레이아웃:
      - Row-major (행 우선): canvas_ptr[row * (CANVAS_WIDTH/8) + (x>>3)]
      - MSB-first: bit7(0x80) = 그룹 내 가장 왼쪽 픽셀
      - pushCanvas()의 읽기 방식:
            byte_idx = gx >> 3
            bit_mask = 0x80 >> (gx & 7)

    SSD1306 페이지모드(열-우선, LSB=위쪽)와 반드시 구분해야 함.

    [최적화] np.packbits() 사용 — 기존 for 루프 대비 약 5배 빠름.
    bitorder='big' (MSB-first, 기본값) → ESP32 펌웨어와 완벽 호환.

    Parameters:
        mono_array: shape (64, 512), 0=검정 255=흰색

    Returns:
        bytes, 길이 4096 (64행 × 64바이트/행)
    """
    bits = (mono_array > 0).view(np.uint8)
    # MSB-first (bitorder='big' 기본값) → 펌웨어 0x80>>(gx&7) 방식과 호환
    return np.packbits(bits, axis=-1).tobytes()

