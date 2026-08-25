#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
为核心 3000 批量生成英文例句,输出 assets/core_examples.tsv
(word<TAB>example)。支持断点续跑、失败重试、分批请求。

用法:
  python3 tools/gen_examples.py \
      --base-url https://api.deepseek.com/v1 \
      --model deepseek-chat \
      --api-key sk-xxxx

只补尚未生成的词;已存在于输出文件的词自动跳过。
"""
import argparse
import csv
import os
import sys
import time
import urllib.request
import urllib.error
import json

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORDS_CSV = os.path.join(ROOT, "assets", "oxford3000.csv")
OUT_TSV = os.path.join(ROOT, "assets", "core_examples.tsv")

BATCH = 20          # 每批请求的单词数
MAX_RETRY = 4
RETRY_WAIT = 8      # 秒


def load_words():
    words = []
    with open(WORDS_CSV, encoding="utf-8") as f:
        reader = csv.reader(f)
        next(reader, None)  # 表头
        for row in reader:
            if len(row) >= 2 and row[1].strip():
                words.append(row[1].strip().lower())
    # 去重保序
    seen = set()
    uniq = []
    for w in words:
        if w not in seen:
            seen.add(w)
            uniq.append(w)
    return uniq


def load_done():
    done = {}
    if os.path.exists(OUT_TSV):
        with open(OUT_TSV, encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or "\t" not in line:
                    continue
                w, ex = line.split("\t", 1)
                done[w.strip().lower()] = ex.strip()
    return done


def make_prompt(batch):
    word_list = "\n".join(batch)
    return (
        "For each English word below, write one short, simple, natural "
        "example sentence that uses the exact word. Output ONLY lines in "
        "this exact format, one per word:\n"
        "word<TAB>sentence\n"
        "Rules:\n"
        "- Use a literal TAB character between the word and the sentence.\n"
        "- Keep each sentence under 14 words.\n"
        "- Do not number lines, add no extra text, no quotes around the "
        "sentence.\n"
        "- Use the exact spelling of the given word.\n\n"
        + word_list
    )


def call_api(base_url, model, api_key, prompt, timeout=120):
    url = base_url.rstrip("/") + "/chat/completions"
    body = json.dumps({
        "model": model,
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.7,
        "max_tokens": 1200,
    }).encode("utf-8")
    req = urllib.request.Request(url, data=body, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Authorization", f"Bearer {api_key}")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        data = json.loads(resp.read().decode("utf-8"))
    return data["choices"][0]["message"]["content"]


def parse_response(text, expected):
    """解析 'word<TAB>sentence' 行,返回 {word: sentence}。"""
    result = {}
    expected_lower = {w.lower() for w in expected}
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        # 模型可能输出字面 <TAB>、真 tab、或多空格分隔
        if "<TAB>" in line:
            w, s = line.split("<TAB>", 1)
        elif "\t" in line:
            w, s = line.split("\t", 1)
        elif "    " in line:
            parts = line.split(None, 1)
            if len(parts) != 2:
                continue
            w, s = parts
        else:
            continue
        w = w.strip().lower().strip(".,:;\"'")
        s = s.strip().strip("\"'")
        if w in expected_lower and s and len(s) < 200:
            result[w] = s
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base-url", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--api-key", required=True)
    ap.add_argument("--limit", type=int, default=0,
                    help="只处理前 N 个未完成词(调试用)")
    args = ap.parse_args()

    words = load_words()
    done = load_done()
    todo = [w for w in words if w not in done]
    if args.limit > 0:
        todo = todo[:args.limit]

    print(f"总词数 {len(words)},已完成 {len(done)},待生成 {len(todo)}",
          flush=True)
    if not todo:
        print("全部完成。")
        return

    out_f = open(OUT_TSV, "a", encoding="utf-8")
    total = len(todo)
    completed = 0

    for i in range(0, total, BATCH):
        batch = todo[i:i + BATCH]
        prompt = make_prompt(batch)

        got = {}
        for attempt in range(1, MAX_RETRY + 1):
            try:
                text = call_api(args.base_url, args.model, args.api_key,
                                prompt)
                got = parse_response(text, batch)
                if got:
                    break
            except (urllib.error.URLError, urllib.error.HTTPError,
                    TimeoutError, KeyError, json.JSONDecodeError) as e:
                print(f"  批次请求失败(第{attempt}次): {e}", flush=True)
            if attempt < MAX_RETRY:
                time.sleep(RETRY_WAIT * attempt)

        if not got:
            print(f"  [跳过] 批次 {batch} 多次失败,留待下次续跑", flush=True)
            continue

        for w in batch:
            if w in got and got[w]:
                out_f.write(f"{w}\t{got[w]}\n")
                done[w] = got[w]
            else:
                print(f"  [缺] {w} 未返回例句", flush=True)
        out_f.flush()
        completed += len(batch)
        ok = sum(1 for w in batch if w in got)
        print(f"进度 {completed}/{total}  (本批 {ok}/{len(batch)})",
              flush=True)

    out_f.close()
    print(f"完成。输出: {OUT_TSV} (共 {len(done)} 条)")


if __name__ == "__main__":
    main()
