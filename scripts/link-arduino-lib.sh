#!/usr/bin/env bash
# DeskStreamCore 라이브러리를 Arduino 스케치북 libraries/ 폴더에 심볼릭 링크로 연결한다 (PC마다 1회).
#
# Arduino IDE / arduino-cli 는 스케치 폴더 밖의 상대경로 include 를 지원하지 않으므로,
# 저장소의 libraries/DeskStreamCore 를 스케치북 libraries/ 에 링크해 모든 스케치가 공유한다.
#
# 사용:  bash scripts/link-arduino-lib.sh [스케치북 경로]
#   스케치북 경로를 생략하면 arduino-cli 설정(directories.user) → ~/Documents/Arduino 순으로 찾는다.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIB_SRC="$REPO/libraries/DeskStreamCore"

if [ $# -ge 1 ]; then
  SKETCHBOOK="$1"
elif command -v arduino-cli >/dev/null 2>&1 && dir=$(arduino-cli config get directories.user 2>/dev/null) && [ -n "$dir" ]; then
  SKETCHBOOK="$dir"
else
  SKETCHBOOK="$HOME/Documents/Arduino"
fi

DEST="$SKETCHBOOK/libraries/DeskStreamCore"
mkdir -p "$SKETCHBOOK/libraries"

if [ -L "$DEST" ]; then
  current="$(readlink "$DEST")"
  if [ "$current" = "$LIB_SRC" ]; then
    echo "✅ 이미 연결됨: $DEST → $LIB_SRC"
    exit 0
  fi
  echo "⚠️  다른 경로로 연결되어 있어 교체합니다: $current"
  rm "$DEST"
elif [ -e "$DEST" ]; then
  echo "❌ $DEST 에 링크가 아닌 폴더/파일이 있습니다. 직접 확인 후 옮기거나 지운 뒤 다시 실행하세요."
  exit 1
fi

ln -s "$LIB_SRC" "$DEST"
echo "✅ 연결 완료: $DEST → $LIB_SRC"
