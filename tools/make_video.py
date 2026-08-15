#!/usr/bin/env python3
"""宣传片剪辑合成：真实录屏 + 动效片段 + 合成配乐。

用法: python3 tools/make_video.py vertical|horizontal
输出: docs/video/source/out-<mode>.mp4
"""

import os
import subprocess
import sys

SRC = "/home/liang/Projects/english3000/docs/video/source"
DEMO = f"{SRC}/demo-full.mp4"
AI = f"{SRC}/ai-scenes"
MUSIC = f"{SRC}/music.wav"
TMP = f"{SRC}/tmp"
FADE = 0.5

# (源, 起始秒, 时长)
SEGMENTS = {
    "vertical": [
        (f"{AI}/01-entry.png", 0, 5),
        (DEMO, 82, 6),
        (f"{AI}/02-characters.png", 0, 5),
        (DEMO, 8, 7),
        (f"{AI}/03-pulse.png", 0, 5),
        (DEMO, 21, 9),
        (f"{AI}/04-merge.png", 0, 5),
        (DEMO, 50, 8),
        (f"{AI}/05-world.png", 0, 5),
        (DEMO, 104, 7),
        (f"{AI}/06-one.png", 0, 5),
    ],
    "horizontal": [
        (f"{AI}/01-entry.png", 0, 7),
        (DEMO, 82, 10),
        (f"{AI}/02-characters.png", 0, 7),
        (DEMO, 8, 12),
        (f"{AI}/03-pulse.png", 0, 7),
        (DEMO, 21, 14),
        (f"{AI}/04-merge.png", 0, 7),
        (DEMO, 50, 16),
        (f"{AI}/05-world.png", 0, 7),
        (DEMO, 104, 14),
        (f"{AI}/06-one.png", 0, 7),
    ],
}


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("CMD FAILED:", " ".join(cmd)[:400])
        print(r.stderr[-3000:])
        sys.exit(1)


def prepare_segments(mode):
    os.makedirs(TMP, exist_ok=True)
    if mode == "vertical":
        W, H = 1080, 1920
    else:
        W, H = 1920, 1080
    vf = (
        f"split=2[b0][f0];"
        f"[b0]scale={W}:{H}:force_original_aspect_ratio=increase,"
        f"crop={W}:{H},boxblur=20:2[b];"
        f"[f0]scale={W}:-2[f];"
        f"[b][f]overlay=(W-w)/2:(H-h)/2,settb=AVTB,fps=30,"
        f"format=yuv420p[v]"
    )
    for i, (src, start, dur) in enumerate(SEGMENTS[mode]):
        out = f"{TMP}/seg{mode}{i}.mp4"
        if src.endswith(".png"):
            # 氛围图：Ken Burns 缓慢推拉，输出 16:9 前景片段
            if mode == "vertical":
                sw, sh = 1080, 608
            else:
                sw, sh = 1920, 1080
            d = int(30 * dur)
            kb = (
                f"zoompan=z='min(zoom+0.0010,1.18)':d={d}:"
                f"x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':"
                f"fps=30:s={sw}x{sh},settb=AVTB,format=yuv420p"
            )
            run([
                "ffmpeg", "-y", "-v", "error",
                "-i", src,
                "-vf", f"{kb},{vf}",
                "-t", str(dur),
                "-c:v", "libx264", "-preset", "fast",
                "-crf", "18", "-r", "30", "-an", out,
            ])
        else:
            run([
                "ffmpeg", "-y", "-v", "error",
                "-ss", str(start), "-t", str(dur), "-i", src,
                "-vf", vf, "-c:v", "libx264", "-preset", "fast",
                "-crf", "18", "-r", "30", "-an", out,
            ])


def concat(mode):
    segs = SEGMENTS[mode]
    durs = [d for _, _, d in segs]
    n = len(segs)
    inputs = []
    for i in range(n):
        inputs += ["-i", f"{TMP}/seg{mode}{i}.mp4"]
    inputs += ["-i", MUSIC]

    fc = []
    prev = "[0:v]"
    offset = 0.0
    for i in range(1, n):
        out = f"[vx{i}]"
        fc.append(f"{prev}[{i}:v]xfade=transition=fade:duration={FADE}"
                  f":offset={offset:.3f}{out}")
        prev = out
        offset += durs[i - 1] - FADE

    if mode == "vertical":
        total = sum(durs) - FADE * (n - 1)
    else:
        total = sum(durs) - FADE * (n - 1)
    fc.append(f"[{n}:a]atrim=0:{total:.2f},asetpts=PTS-STARTPTS,"
              f"volume=0.16,afade=t=in:st=0:d=1.5,"
              f"afade=t=out:st={total-2.5:.2f}:d=2.5[a]")
    out = f"{SRC}/out-{mode}.mp4"
    run([
        "ffmpeg", "-y", "-v", "error", *inputs,
        "-filter_complex", ";".join(fc),
        "-map", f"[vx{n - 1}]",
        "-map", "[a]",
        "-c:v", "libx264", "-preset", "medium", "-crf", "19",
        "-c:a", "aac", "-b:a", "128k",
        "-t", f"{total:.2f}", out,
    ])
    print("written", out, round(total, 1), "s")


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "vertical"
    if mode not in SEGMENTS:
        print("mode must be vertical|horizontal")
        sys.exit(1)
    prepare_segments(mode)
    concat(mode)


if __name__ == "__main__":
    main()
