#!/usr/bin/env python3
"""批量给词库中词形表缺失的词生成词形变化（可断点续跑）。

用法：python3 tools/generate_inflections.py [数据库路径] [每批词数]
"""

import json
import os
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
SECOND_MODEL = os.environ.get("EX_SECOND_MODEL", "qwen2.5:3b")


def generate(prompt: str, model: str = MODEL) -> str:
    body = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "think": False,
        "options": {"num_predict": BATCH * 60 + 300, "temperature": 0.4},
    }
    req = urllib.request.Request(
        URL, data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as resp:
        return json.load(resp)["response"]


def parse_line(line: str):
    m = re.match(r"^([a-z'\-]+)\s*[:：]\s*(.+)$", line.strip().lower())
    if not m:
        return None
    word = m.group(1)
    forms = [f.strip() for f in re.split(r"[,，]", m.group(2))]
    valid = []
    for f in forms:
        if (f and f != word and len(f) >= 2
                and all(c.isalpha() or c in "-'" for c in f)):
            valid.append(f)
    return word, valid


def main() -> int:
    con = sqlite3.connect(DB)
    words = [w for (w, _) in con.execute(
        "SELECT DISTINCT w.word, w.pos FROM words w "
        "WHERE w.word NOT IN (SELECT lemma FROM word_forms) "
        "AND (w.pos='' OR NOT (w.pos LIKE 'conj%' OR w.pos LIKE 'prep%' "
        "OR w.pos LIKE 'adv%' OR w.pos LIKE 'art%' OR w.pos LIKE 'pron%' "
        "OR w.pos LIKE 'int%' OR w.pos LIKE 'num%' OR w.pos LIKE 'det%' "
        "OR w.pos LIKE 'aux%'))") if
        re.fullmatch(r"[a-z'\-]{2,}", w.lower())]
    print(f"缺词形的词：{len(words)} 个", flush=True)
    if not words:
        print("全部都有词形，无需生成", flush=True)
        return 0

    done = 0
    pending = []
    i = 0
    start = time.time()
    while i < len(words):
        batch = words[i:i + BATCH]
        prompt = (
            "For each word below, list its inflected forms "
            "(plural, past tense, past participle, -ing, third person "
            "singular, comparative, superlative as applicable). "
            "Output one line per word in this format: word: form1, form2, "
            "form3. Only output the forms after the colon, no explanations.\n"
            + "".join(f"{j + 1}. {w}\n" for j, w in enumerate(batch)))
        try:
            resp = generate(prompt)
        except Exception as exc:
            print(f"批次失败（稍后重试）: {exc}", flush=True)
            time.sleep(3)
            continue
        for line in resp.splitlines():
            parsed = parse_line(line)
            if not parsed:
                continue
            word, forms = parsed
            if word in batch and forms:
                rows = [(f, word) for f in forms]
                con.executemany(
                    "INSERT OR IGNORE INTO word_forms(form, lemma) "
                    "VALUES(?, ?)", rows)
                done += 1
        con.commit()
        i += BATCH
        if i % (BATCH * 10) < BATCH or i >= len(words):
            elapsed = time.time() - start
            print(f"进度 {i}/{len(words)}  已生成 {done}  "
                  f"速度 {done/elapsed:.1f} 词/秒", flush=True)

    # 第二轮：漏掉的词逐个用大模型补
    missing = [w for (w, _) in con.execute(
        "SELECT DISTINCT w.word, w.pos FROM words w "
        "WHERE w.word NOT IN (SELECT lemma FROM word_forms) "
        "AND (w.pos='' OR NOT (w.pos LIKE 'conj%' OR w.pos LIKE 'prep%' "
        "OR w.pos LIKE 'adv%' OR w.pos LIKE 'art%' OR w.pos LIKE 'pron%' "
        "OR w.pos LIKE 'int%' OR w.pos LIKE 'num%' OR w.pos LIKE 'det%' "
        "OR w.pos LIKE 'aux%'))") if
        re.fullmatch(r"[a-z'\-]{2,}", w.lower())]
    if missing:
        print(f"第二轮补漏 {len(missing)} 个…", flush=True)
        for w in missing:
            prompt = (
                f'List the inflected forms of the English word "{w}". '
                f'Output only the forms separated by commas, '
                f'no explanations.')
            try:
                resp = generate(prompt, SECOND_MODEL)
            except Exception:
                continue
            forms = [f.strip() for f in re.split(r"[,，]", resp)]
            valid = [f for f in forms if f and f.lower() != w.lower()
                     and len(f) >= 2
                     and all(c.isalpha() or c in "-'" for c in f)]
            if valid:
                con.executemany(
                    "INSERT OR IGNORE INTO word_forms(form, lemma) "
                    "VALUES(?, ?)", [(f.lower(), w.lower()) for f in valid])
                con.commit()
    print("完成", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
