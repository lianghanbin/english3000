#!/usr/bin/env python3
"""批量给词库生成例句（只填空缺，可断点续跑）。

用法：python3 tools/generate_examples.py [数据库路径] [每批词数]
      python3 tools/generate_examples.py <数据库> <批数> --clear-article-examples
      （先清掉从文章里提取的例句，再全部用 AI 重新生成）
"""

import json
import re
import sqlite3
import sys
import time
import urllib.request
import os

DB = sys.argv[1] if len(sys.argv) > 1 else (
    "/home/liang/.local/share/liang/english3000/english3000.db")
BATCH = int(sys.argv[2]) if len(sys.argv) > 2 else 10
CLEAR_ARTICLE = "--clear-article-examples" in sys.argv
URL = "http://127.0.0.1:11434/api/generate"
MODEL = "qwen2.5:3b"
SECOND_MODEL = os.environ.get("EX_SECOND_MODEL", "qwen3:14b")


def generate(prompt: str, model: str = MODEL) -> str:
    body = {
        "model": model,
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


def is_valid(word: str, sentence: str) -> bool:
    del word
    s = sentence.strip()
    if len(s) < 12:
        return False
    tokens = s.split()
    if len(tokens) < 4:
        return False
    if not any(len(t) >= 5 for t in tokens):
        return False
    return True


def main() -> int:
    con = sqlite3.connect(DB)
    con.execute("PRAGMA journal_mode=WAL")
    form_to_lemma = {}
    try:
        for form, lemma in con.execute(
                "SELECT form, lemma FROM word_forms"):
            form_to_lemma[form.lower()] = lemma.lower()
    except sqlite3.OperationalError:
        pass

    if CLEAR_ARTICLE:
        articles = [r[0] for r in con.execute(
            "SELECT content FROM articles")]
        cleared = 0
        for wid, sentence in con.execute(
                "SELECT id, example_sentence FROM words "
                "WHERE example_sentence != ''"):
            low = sentence.lower()
            if any(low in content.lower() for content in articles):
                con.execute(
                    "UPDATE words SET example_sentence='' WHERE id=?",
                    (wid,))
                cleared += 1
        con.commit()
        print(f"已清掉文章例句 {cleared} 个，等待 AI 重新生成", flush=True)
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

    def matches(word: str, sentence: str) -> bool:
        low = sentence.lower()
        if word.lower() in low:
            return True
        for token in re.findall(r"[a-z']+", low):
            if form_to_lemma.get(token) == word.lower():
                return True
        return False
    while i < len(words):
        batch = words[i:i + BATCH]
        prompt = (
            "Write one short, simple English sentence for each word below, "
            "using the exact word. Output ONLY one sentence per line, "
            "in the same order. Each sentence must be a complete sentence "
            "with at least 6 words.\n"
            + "".join(f"{j + 1}. {w}\n" for j, (_, w) in enumerate(batch)))
        try:
            sentences = parse_sentences(generate(prompt))
        except Exception as exc:
            print(f"批次失败（稍后重试）: {exc}", flush=True)
            time.sleep(3)
            continue

        for k, (wid, w) in enumerate(batch):
            s = sentences[k].strip() if k < len(sentences) else ""
            if s and matches(w, s) and is_valid(w, s):
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
                f'"{w}". Use the exact word. The sentence must be complete '
                f'with at least 6 words. Output only the sentence.')
            try:
                sentences = parse_sentences(generate(prompt, SECOND_MODEL))
            except Exception:
                continue
            s = sentences[0].strip() if sentences else ""
            if s and matches(w, s) and is_valid(w, s):
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
