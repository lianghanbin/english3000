# English 3000

3000 高频词学习助手（C++ / Qt 6 桌面版）。

词表来自 Oxford 3000（美式英语），按真实语料频率排序，中文释义来自 ECDICT
开源词典。学习数据保存在本地 SQLite，不联网、无账号。

## 功能

- 词表即学习对象：选中哪个词表，学习页就学哪个；每个词表独立记录
  未学 / 待复习 / 已掌握进度，无每日上限、无新词队列
- 学习 + 复习：学习=还没学过的词；点"不认识"后自动进入复习队列，
  复习=学过但没记住的词，可随时返回，进度自动保存
- 阅读优先：内置示例文章 + 阅读器，生词自动高亮（红色=词表外，蓝色=未掌握，
  黑色=已掌握），点击任意词即时查释义、加入独立的「阅读生词」词表
- AI 文章：本地 ollama 按主题生成或把粘贴文章改写成指定难度，后台生成完成后
  通知，文章自动统计当前词表覆盖率
- 领域词表：AI 按领域生成（如 Linux、医学、六级）、从文章提取、手动导入，
  互相独立；列表可拖拽排序，「核心 3000」默认置顶
- 词表检索：搜索单词或中文释义，可标记"已会"、重置、手动添加生词
- 翻译联动：全局热键翻译小窗中遇到的生词，自动加入独立的「翻译生词」词表
- 进度统计：当前词表的未学 / 待复习 / 已掌握数、连续学习天数、阅读覆盖率曲线
- CSV 导入：支持导入任意四列词表（序号、单词、词性、中文释义）

## 构建

依赖：g++、CMake、Qt 6（Widgets + Sql + Network，Debian 装 `qt6-base-dev` 即可）。
AI 文章功能需要本机 ollama 服务或 OpenAI 兼容接口（默认 `http://127.0.0.1:11434`，
模型 `qwen2.5:1.5b`，可在设置页修改）。

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 运行与测试

```bash
./build/english3000    # 启动
./build/test_core      # 单元测试
```

首次启动会自动导入 `assets/oxford3000.csv`（2971 个词）并默认选中
"核心 3000"词表，同时导入词形表（`lemma.en.txt`，用于识别 files/file
这类变形）和 3 篇示例文章。

## 快捷键

| 按键         | 作用             |
| ------------ | ---------------- |
| Ctrl+N       | 学习未学的单词   |
| Ctrl+R       | 复习不认识的单词 |
| 空格 / Enter | 显示释义         |
| 1            | 不认识           |
| 2            | 认识             |

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
src/core.cpp        核心逻辑（词库、词表进度、统计，无 UI 依赖）
src/mainwindow.cpp  Qt Widgets 界面
tests/test_core.cpp 核心逻辑单元测试
assets/             词表 CSV 与图标
```

## 许可证

本项目以 **GPL-3.0** 协议开源：你可以自由使用、修改、分发，
但修改后的版本必须同样以 GPL 协议开源，并保留版权声明。
完整条款见 [LICENSE](LICENSE)。
