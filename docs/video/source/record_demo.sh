#!/bin/bash
# English 3000 宣传片真实录屏：xdotool 慢速演示 + ffmpeg 窗口区域录制
# 用法: bash record_demo.sh test|full

set -u
MODE="${1:-test}"
OUT="demo-${MODE}.mp4"
DUR=30
[ "$MODE" = "full" ] && DUR=140

# 动态定位主窗口并固定到 (100,100)
WID=$(xdotool search --name "English 3000" | head -1)
if [ -z "$WID" ]; then echo "window not found"; exit 1; fi
xdotool windowmove "$WID" 100 100
xdotool windowsize "$WID" 980 881
sleep 0.8
wmctrl -i -a "$WID" || xdotool windowactivate "$WID"
sleep 0.5
eval "$(xdotool getwindowgeometry --shell "$WID")"
WX=$X; WY=$Y; WW=$WIDTH; WH=$HEIGHT

# 标签坐标
TAB_WORDLISTS=$((WX + 120)); TAB_LEARN=$((WX + 40)); TAB_READ=$((WX + 200))
TAB_Y=$((WY + 46))
LIST_X=$((WX + 120))
ITEM1_Y=$((WY + 96)); ITEM2_Y=$((WY + 132))

ffmpeg -y -v error -f x11grab -framerate 30 -video_size "${WW}x${WH}" \
  -i ":0.0+${WX},${WY}" -t "$DUR" -c:v libx264 -preset fast -crf 18 "$OUT" &
FFPID=$!

act() { wmctrl -i -a "$WID" || xdotool windowactivate "$WID"; sleep 0.2; }
wait_t() { sleep "$1"; }

# 0-5s 静止
wait_t 4; act

# 词表：点标签 -> 第2项 -> 回第1项
xdotool mousemove --sync "$TAB_WORDLISTS" "$TAB_Y"; sleep 0.6
xdotool click 1; wait_t 2
xdotool mousemove --sync "$LIST_X" "$ITEM2_Y"; sleep 0.8
xdotool click 1; wait_t 3
xdotool mousemove --sync "$LIST_X" "$ITEM1_Y"; sleep 0.8
xdotool click 1; wait_t 3

# 学习：回学习标签 -> Ctrl+N 卡片流 -> 空格/2/1
xdotool mousemove --sync "$TAB_LEARN" "$TAB_Y"; sleep 0.6
xdotool click 1; wait_t 2
xdotool key ctrl+n; wait_t 3
xdotool key space; wait_t 4
xdotool key 2; wait_t 3
xdotool key space; wait_t 4
xdotool key 2; wait_t 3
xdotool key space; wait_t 4
xdotool key 1; wait_t 3

if [ "$MODE" = "full" ]; then
  xdotool key Escape; wait_t 2
  # 阅读
  xdotool mousemove --sync "$TAB_READ" "$TAB_Y"; sleep 0.6
  xdotool click 1; wait_t 2
  xdotool mousemove --sync $((WX + 170)) $((WY + 96)); sleep 0.6
  xdotool click 1; wait_t 4
  xdotool mousemove --sync $((WX + 620)) $((WY + 220)); sleep 0.8
  xdotool click 1; wait_t 3
  xdotool mousemove --sync $((WX + 700)) $((WY + 300)); sleep 0.8
  xdotool click 1; wait_t 3
  xdotool mousemove --sync $((WX + 620)) $((WY + 420)); sleep 0.8
  xdotool click 5; wait_t 3
  # 翻译
  xdotool key ctrl+alt+t; wait_t 2
  TW=$(xdotool search --name "翻译" | head -1)
  if [ -n "$TW" ]; then
    eval $(xdotool getwindowgeometry --shell "$TW")
    IX=$((X + 200)); IY=$((Y + 110))
    BY=$((Y + 370)); BX=$((X + 50))
    xdotool mousemove --sync "$IX" "$IY"; sleep 0.6
    xdotool click 1; sleep 0.5
    xdotool type --delay 60 "The quick brown fox jumps over the lazy dog."
    wait_t 2
    xdotool mousemove --sync "$BX" "$BY"; sleep 0.6
    xdotool click 1
    wait_t 20
    xdotool windowclose "$TW"
  fi
  # 词表沉淀
  xdotool mousemove --sync "$TAB_WORDLISTS" "$TAB_Y"; sleep 0.6
  xdotool click 1; wait_t 2
  xdotool mousemove --sync "$LIST_X" "$ITEM2_Y"; sleep 0.6
  xdotool click 1; wait_t 4
  xdotool mousemove --sync "$TAB_LEARN" "$TAB_Y"; sleep 0.6
  xdotool click 1; wait_t 3
fi

wait_t 2
kill "$FFPID" 2>/dev/null
wait "$FFPID" 2>/dev/null
echo "recorded $OUT"
