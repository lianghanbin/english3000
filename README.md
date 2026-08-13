# English 3000

3000 高频词学习助手（C++ / Qt 6 桌面版）。

词表来自 Oxford 3000（美式英语），按真实语料频率排序，中文释义来自 ECDICT
开源词典。学习数据保存在本地 SQLite，不联网、无账号。

## 功能

- 每日新词：按设置的每日数量（默认 25 个）从词频最高的开始
- 间隔重复：Leitner 6 盒调度，认识的词间隔越来越长，不认识的自动回炉
- 阅读优先：内置示例文章 + 阅读器，生词自动高亮（红色=词表外，蓝色=未掌握），
  点击任意词即时查释义、加入生词池
- AI 文章：本地 ollama 按主题生成或把粘贴文章改写成指定难度，后台生成完成后
  通知并自动入库，文章自动统计词汇覆盖率
- 生词池：阅读中收集的生词可手动挑选加入当日新词队列，复习卡片带原文例句
- 词表检索：搜索单词或中文释义，可标记"已会"、重置、手动添加阅读时遇到的生词
- 进度统计：已掌握数、今日复习、连续学习天数
- CSV 导入：支持导入任意四列词表（序号、单词、词性、中文释义）

## 构建

依赖：g++、CMake、Qt 6（Widgets + Sql + Network，Debian 装 `qt6-base-dev` 即可）。
AI 文章功能需要本机 ollama 服务（默认 `http://127.0.0.1:11434`，模型 `qwen3:14b`，
可在设置页修改）。

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 运行与测试

```bash
./build/english3000    # 启动
./build/test_core      # 单元测试
```

首次启动会自动导入 `assets/oxford3000.csv`（2971 个词）、词形表
（`lemma.en.txt`，用于识别 files/file 这类变形）和 3 篇示例文章。

## 快捷键

| 按键 | 作用 |
|------|------|
| Ctrl+N | 开始新词 |
| Ctrl+R | 开始复习 |
| 空格 / Enter | 显示释义 |
| 1 | 不认识 |
| 2 | 认识 |

## 数据位置

- 数据库：`~/.local/share/liang/english3000/english3000.db`
- 可在"设置"页重置全部进度或重新导入词表
- 配套学习计划与阅读路线：`~/Documents/english-3000/`

## 分发（AppImage）

根目录的 `English_3000-x86_64.AppImage` 是便携包：

```bash
chmod +x English_3000-x86_64.AppImage
./English_3000-x86_64.AppImage
```

已内置：程序本体、Qt 运行库、牛津 3000 词表、词形表与示例文章。
首次启动自动创建数据目录并导入词表。

运行时外部依赖（不打包，按需安装）：

- AI 功能：本地 ollama 服务（默认 `http://127.0.0.1:11434`）
- 朗读：piper（推荐）或 espeak-ng
- 截图翻译 OCR：tesseract-ocr（eng）

如果 AppImage 无法挂载（FUSE 问题），用 `--appimage-extract-and-run` 运行。

## 项目结构

```text
src/core.cpp        核心逻辑（词库、Leitner 调度、统计，无 UI 依赖）
src/mainwindow.cpp  Qt Widgets 界面
tests/test_core.cpp 核心逻辑单元测试
assets/             词表 CSV 与图标
```
