#!/usr/bin/env python3
"""批量给词库生成例句（只填空缺，可断点续跑）。

用法：python3 tools/generate_examples.py [数据库路径] [每批词数]
"""

import json
import re
import sqlite3
import sys
import time
import urllib.request

DB = sys.argv[1] if len(sys.argv) > 1 else (
    "/home/liang/.local/share/liang/english3000/english3000.db")
BATCH = int(sys.argv[2]) if len(sys.argv) > 2 else 10
URL = "http://127.0.0.1:11434/api/generate"
MODEL = "qwen2.5:3b"


def generate(prompt: str) -> str:
    body = {
        "model": MODEL,
        "prompt": prompt,
        "stream": False,
        "think": False,
        "options": {
            "num_predict": BATCH * 45 + 200,
            "temperature": 0.5,
        },
    }
    req = urllib.request.Request(
        URL, data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as resp:
        return json.load(resp)["response"]


def parse_sentences(resp: str):
    lines = [l.strip() for l in resp.splitlines() if l.strip()]
    out = []
    for line in lines:
        s = re.sub(r"^\d+[.、:)\s]+", "", line).strip()
        if s:
            out.append(s)
    return out


def main() -> int:
    con = sqlite3.connect(DB)
    con.execute("PRAGMA journal_mode=WAL")
    words = con.execute(
        "SELECT id, word FROM words WHERE example_sentence='' "
        "ORDER BY rank, id").fetchall()
    total = len(words)
    print(f"待生成例句：{total} 个，每批 {BATCH} 个", flush=True)
    if total == 0:
        print("全部已有例句，无需生成", flush=True)
        return 0

    done = 0
    skipped = 0
    pending = []
    i = 0
    start = time.time()
    while i < len(words):
        batch = words[i:i + BATCH]
        prompt = (
            "Write one short, simple English sentence for each word below, "
            "using the exact word. Output ONLY one sentence per line, "
            "in the same order.\n"
            + "".join(f"{j + 1}. {w}\n" for j, (_, w) in enumerate(batch)))
        try:
            sentences = parse_sentences(generate(prompt))
        except Exception as exc:
            print(f"批次失败（稍后重试）: {exc}", flush=True)
            time.sleep(3)
            continue

        for k, (wid, w) in enumerate(batch):
            s = sentences[k].strip() if k < len(sentences) else ""
            if s and w.lower() in s.lower():
                con.execute(
                    "UPDATE words SET example_sentence=? WHERE id=?",
                    (s, wid))
                done += 1
            else:
                skipped += 1
                pending.append((wid, w))
        con.commit()
        i += BATCH

        if i % (BATCH * 10) < BATCH or i >= total:
            elapsed = time.time() - start
            speed = done / elapsed if elapsed > 0 else 0
            eta = (total - i) / speed / 60 if speed > 0 else 0
            print(
                f"进度 {i}/{total}  已生成 {done}  跳过 {skipped}  "
                f"速度 {speed:.1f} 词/秒  预计还需 {eta:.0f} 分钟",
                flush=True)

    con.commit()

    # 第二轮：漏掉的词逐个补
    if pending:
        print(f"第二轮补漏 {len(pending)} 个…", flush=True)
        for wid, w in pending:
            prompt = (
                f'Write one short, simple English sentence using the word '
                f'"{w}". Use the exact word. Output only the sentence.')
            try:
                sentences = parse_sentences(generate(prompt))
            except Exception:
                continue
            s = sentences[0].strip() if sentences else ""
            if s and w.lower() in s.lower():
                con.execute(
                    "UPDATE words SET example_sentence=? WHERE id=?",
                    (s, wid))
                done += 1
                skipped -= 1
        con.commit()
    print(f"完成：生成 {done}，跳过/失败 {skipped}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
