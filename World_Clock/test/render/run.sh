#!/bin/bash
# worynim@gmail.com
# 화면 미리보기: 장치와 같은 렌더러를 PC에서 돌려 PNG로 저장한다.
#   사용: bash test/render/run.sh [결과 폴더]   (기본: 임시 폴더, U8g2 오브젝트는 그 안에 캐시)
#   필요: g++, python3 + Pillow, Arduino 스케치북의 U8g2 라이브러리 (U8G2_DIR 로 바꿀 수 있다)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-${TMPDIR:-/tmp}/world_clock_render}"
U8G2_DIR="${U8G2_DIR:-$HOME/Documents/Arduino/libraries/U8g2/src}"
mkdir -p "$OUT/obj"

# U8g2 C 소스는 한 번만 컴파일한다 (글꼴 파일이 커서 오래 걸린다)
if [ ! -f "$OUT/obj/libu8g2.a" ]; then
    (cd "$OUT/obj" && cc -c -O1 -w -DU8G2_USE_LARGE_FONTS -I"$U8G2_DIR/clib" "$U8G2_DIR"/clib/*.c && ar rcs libu8g2.a *.o)
fi
g++ -std=c++17 -O1 -Wall -DU8G2_USE_LARGE_FONTS -I"$U8G2_DIR" -I"$U8G2_DIR/clib" \
    "$HERE/render_main.cpp" "$HERE/demo_pages.cpp" "$HERE"/../../{clock_view,setup_view,misc_view,fonts}.cpp \
    "$OUT/obj/libu8g2.a" -o "$OUT/render"
"$OUT/render" "$OUT"

# PBM → 4배 확대 PNG (화면 사이에 틈을 넣어 실제 배치처럼 보이게)
python3 - "$OUT" <<'PY'
import glob, os, sys
from PIL import Image, ImageOps
out = sys.argv[1]
for p in sorted(glob.glob(os.path.join(out, "*.pbm"))):
    src = ImageOps.invert(Image.open(p).convert("L"))      # PBM 은 1 = 검정 → 켜진 점을 밝게
    k, gap = 4, 12
    dst = Image.new("L", (512 * k + gap * 5, 64 * k + gap * 2), 60)
    for s in range(4):
        tile = src.crop((s * 128, 0, s * 128 + 128, 64)).resize((128 * k, 64 * k), Image.NEAREST)
        dst.paste(tile, (gap + s * (128 * k + gap), gap))
    dst.save(p[:-4] + ".png")
    print(p[:-4] + ".png")
PY
