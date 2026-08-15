#!/usr/bin/env python3
"""用本机 ComfyUI 批量生成宣传片 6 幕氛围图（动漫插画风，无文字）。"""

import json
import time
import urllib.request
import uuid
import glob
import os
import shutil

API = "http://127.0.0.1:8188"
OUT_DIR = "/home/liang/Projects/english3000/docs/video/source/ai-scenes"

STYLE = ("quiet minimal anime style illustration, soft light, low saturation, "
         "no people, no text, no watermark, clean composition")

SCENES = [
    ("01-entry", "a dim cold blue study desk with a faint glowing translation "
     "box dissolving into thin air, hesitant quiet mood"),
    ("02-characters", "many small neat squares arranged like a grid of "
     "characters, warm light gradually glowing, sense of a library of "
     "building blocks"),
    ("03-pulse", "a single glowing green point in darkness with soft "
     "concentric ripples spreading, sense of a tap and pulse"),
    ("04-merge", "dozens of small green blocks floating and converging into "
     "a long line like a sentence, sense of words merging into text"),
    ("05-world", "a narrow door crack bursting with warm white light opening "
     "into a vast bright world, sense of stepping into a new world"),
    ("06-one", "a single tiny point of light in dark space gently glowing, "
     "everything fading around it, peaceful ending"),
]


def post(payload):
    req = urllib.request.Request(
        API + "/prompt", json.dumps(payload).encode(),
        {"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=600).read())


def build_workflow(text, seed, prefix):
    return {
        "3": {"class_type": "KSampler", "inputs": {
            "seed": seed, "steps": 28, "cfg": 6.0,
            "sampler_name": "euler", "scheduler": "normal", "denoise": 1.0,
            "model": ["4", 0], "positive": ["6", 0], "negative": ["7", 0],
            "latent_image": ["5", 0]}},
        "4": {"class_type": "CheckpointLoaderSimple", "inputs": {
            "ckpt_name": "Counterfeit-V2.5_fp16.safetensors"}},
        "5": {"class_type": "EmptyLatentImage", "inputs": {
            "width": 832, "height": 480, "batch_size": 1}},
        "6": {"class_type": "CLIPTextEncode", "inputs": {
            "text": text + ", " + STYLE, "clip": ["4", 1]}},
        "7": {"class_type": "CLIPTextEncode", "inputs": {
            "text": "blurry, low quality, messy, cluttered, text, watermark",
            "clip": ["4", 1]}},
        "8": {"class_type": "VAEDecode", "inputs": {
            "samples": ["3", 0], "vae": ["4", 2]}},
        "9": {"class_type": "SaveImage", "inputs": {
            "filename_prefix": prefix, "images": ["8", 0]}},
    }


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    client = str(uuid.uuid4())
    for i, (name, text) in enumerate(SCENES):
        prefix = "e3_" + name
        post({"prompt": build_workflow(text, 1000 + i, prefix),
              "client_id": client})
        print("submitted", name)

    # 等待 6 张全部生成
    for _ in range(180):
        files = sorted(glob.glob(
            "/home/liang/Projects/ComfyUI/output/e3_*.png"))
        if len(files) >= 6:
            break
        time.sleep(2)
    print("generated", len(files))
    for f in files:
        base = os.path.basename(f)
        name = base.split("_")[1]
        dst = os.path.join(OUT_DIR, name + ".png")
        shutil.copy(f, dst)
        print("->", dst)


if __name__ == "__main__":
    main()
