#!/usr/bin/env python3
"""English 3000 宣传片配乐合成（numpy）。

输出 docs/video/source/music.wav（44.1kHz 16bit，约 120 秒）。
情绪：intro 迟疑(0-18s) -> build 探索(18-36s) -> bright 开阔(36-62s)
      -> outro 宁静(62-120s)。
"""

import numpy as np
import wave

SR = 44100


def freq(name):
    table = {
        "G2": 98.00, "A2": 110.00,
        "C3": 130.81, "D3": 146.83, "E3": 164.81, "F3": 174.61,
        "G3": 196.00, "A3": 220.00, "B3": 246.94,
        "C4": 261.63, "D4": 293.66, "E4": 329.63, "F4": 349.23,
        "G4": 392.00, "A4": 440.00, "B4": 493.88,
        "C5": 523.25, "D5": 587.33, "E5": 659.26, "G5": 783.99,
        "A5": 880.00,
    }
    return table[name]


def fade_in_out(sig, attack=1.5, release=2.0):
    n = sig.size
    a = int(attack * SR)
    r = int(release * SR)
    if a > 0:
        sig[:a] *= np.linspace(0, 1, a)
    if r > 0:
        sig[-r:] *= np.linspace(1, 0, r)
    return sig


def pad(chord, dur, amp=0.07):
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    for f in chord:
        out += np.sin(2 * np.pi * f * t)
        out += 0.35 * np.sin(2 * np.pi * f * 1.003 * t)
        out += 0.25 * np.sin(2 * np.pi * f * 0.5 * t)
    out *= amp
    return fade_in_out(out)


def pluck(f, dur=2.0, amp=0.12):
    n = int(dur * SR)
    t = np.arange(n) / SR
    s = np.sin(2 * np.pi * f * t) + 0.35 * np.sin(4 * np.pi * f * t)
    s *= np.exp(-t * 2.0)
    return s * amp


def bass(f, dur, amp=0.10):
    n = int(dur * SR)
    t = np.arange(n) / SR
    s = np.sign(np.sin(2 * np.pi * f * t)) * np.exp(-t * 0.8)
    s += np.sin(2 * np.pi * f * t) * 0.5
    return s * amp * fade_in_out(np.ones(n), 0.02, 0.3)


def kick(amp=0.25):
    n = int(0.22 * SR)
    t = np.arange(n) / SR
    f = 150 * np.exp(-t * 22) + 42
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 13) * amp


def hat(amp=0.04):
    n = int(0.05 * SR)
    rng = np.random.default_rng(7)
    s = np.diff(rng.standard_normal(n), prepend=0)
    s *= np.exp(-np.arange(n) / (0.012 * SR))
    return s * amp


def place(sig, at, total):
    start = int(at * SR)
    out = np.zeros(int(total * SR))
    end = min(start + sig.size, out.size)
    if start < out.size:
        out[start:end] += sig[: end - start]
    return out


def main():
    Am = [freq(n) for n in ("A3", "C4", "E4")]
    F = [freq(n) for n in ("F3", "A3", "C4")]
    C = [freq(n) for n in ("C3", "E3", "G3")]
    G = [freq(n) for n in ("G3", "B3", "D4")]

    total = 120.0
    tracks = []

    # ---- intro：迟疑、安静 ----
    tracks.append(place(pad(Am, 18.0, amp=0.055), 0, total))
    for t in (2.0, 7.0, 12.5):
        tracks.append(place(pluck(freq("E4"), 2.5, 0.10), t, total))

    # ---- build：探索，节奏渐起 ----
    for i, (chord, mel, b) in enumerate(
        [(F, "A4", 55), (C, "G4", 49), (F, "C5", 55), (C, "G4", 49)]):
        start = 18.0 + i * 4.5
        tracks.append(place(pad(chord, 4.6, amp=0.065), start, total))
        tracks.append(place(pluck(freq(mel), 1.8, 0.11), start + 0.4, total))
        tracks.append(place(bass(b, 2.0, 0.08), start + 2.0, total))

    # ---- bright：开阔，4/4 节拍 ----
    for i in range(8):
        start = 36.0 + i * 3.0
        chord = Am if i % 2 == 0 else G
        tracks.append(place(pad(chord, 3.1, amp=0.075), start, total))
        tracks.append(place(kick(0.26), start, total))
        tracks.append(place(kick(0.16), start + 1.5, total))
        tracks.append(place(hat(0.035), start, total))
        tracks.append(place(hat(0.025), start + 0.75, total))
        mel = "E5" if i % 2 == 0 else "D5"
        tracks.append(place(pluck(freq(mel), 2.2, 0.10), start + 0.5, total))
        tracks.append(place(bass(freq("A2" if chord is Am else "G2"), 2.0, 0.09),
                            start, total))

    # ---- outro：宁静，渐弱 ----
    tracks.append(place(pad(Am, 58.0, amp=0.06), 62, total))
    for t in (64.0, 72.0, 80.0, 90.0, 100.0, 108.0):
        tracks.append(place(pluck(freq("E4"), 3.0, 0.085), t, total))

    out = np.zeros(int(total * SR))
    for tr in tracks:
        out += tr
    out = out / (np.max(np.abs(out)) + 1e-9) * 0.55
    fade = int(5 * SR)
    out[-fade:] *= np.linspace(1, 0, fade)

    pcm = (out * 32767).astype(np.int16)
    with wave.open(
        "/home/liang/Projects/english3000/docs/video/source/music.wav", "wb"
    ) as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print("music.wav written:", round(pcm.size / SR, 1), "s")


if __name__ == "__main__":
    main()
