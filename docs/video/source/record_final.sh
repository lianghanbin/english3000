#!/bin/bash
# 宣传片全片录制：应用 --demo 时间线自驱动，ffmpeg 录窗口区域 140s
set -u

# 关掉已运行的实例（避免单实例锁）
for p in $(pgrep -x english3000); do kill "$p"; done
sleep 1

cd /home/liang/Projects/english3000
setsid nohup ./build/english3000 --demo >/tmp/english3000_demo.log 2>&1 &
sleep 4

WID=$(xdotool search --name "English 3000" | head -1)
if [ -z "$WID" ]; then
    echo "window not found"; exit 1
fi
xdotool windowmove "$WID" 100 100
xdotool windowsize "$WID" 980 881
wmctrl -i -a "$WID"
sleep 1

ffmpeg -y -v error -f x11grab -framerate 30 -video_size 980x881 \
  -i :0.0+100,100 -t 140 -c:v libx264 -preset fast -crf 18 \
  /home/liang/Projects/english3000/docs/video/source/demo-full.mp4

echo "recorded demo-full.mp4"
