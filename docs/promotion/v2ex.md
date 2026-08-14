# V2EX 发帖文案（分享创造）

## 标题

开源一个本地优先的 AI 英语学习工具（C++/Qt，Windows/Linux/Android）

## 正文

断断续续做了一年多，终于敢拿出来见人了：一个以“词表即学习对象”为核心的
英语学习软件，核心思路是——**不联网、不订阅、数据全在本地，AI 内容自己生成。**

### 它解决什么问题

市面上的背单词软件（墨墨、不背、Anki）要么订阅、要么例句靠人工词书，
要么背单词和阅读是割裂的。我想做一个闭环：

1. 选一个词表（内置核心 3000，也可以 AI 生成领域词表、从文章提取、导入 CSV）
2. 学习页直接学这个词表：认识 / 不认识，不认识自动进复习队列
3. 阅读 AI 生成的分级文章，生词四色高亮（当前词表/其他词表/已掌握/未入表）
4. 阅读和翻译中遇到的生词，一键收进独立词表，再回流到学习

### 技术栈

- C++20 / Qt 6，一套代码覆盖桌面（Widgets）和移动端（QML）
- 本地 AI：llama.cpp + Qwen2.5 1.5B，CUDA → Vulkan → CPU 自动回退，
  也可以接任意 OpenAI 兼容云端 API（DeepSeek/通义/GLM/Kimi）
- 翻译：全局热键小窗 + 截图翻译（Linux 下 OCR）
- 更新：应用内检查更新，下载后 SHA256 校验、自动安装重启
- Windows 有一键安装包（内置小模型，装完即用）；Android APK 自动发布

### 下载

- 仓库：https://github.com/lianghanbin/english3000
- v1.1.0 Release：https://github.com/lianghanbin/english3000/releases/tag/v1.1.0
- Windows 一键包（含本地模型）：https://github.com/lianghanbin/english3000/releases/download/win-oneclick/English3000-OneClick-Setup.exe

### 已知不足（诚实说）

- 本地 1.5B 模型适合短句翻译/例句/简单文章，长文翻译建议换云端 API
- 安卓端还缺数据统计页，进度同步也还没做
- 没有记忆曲线算法，复习是简单二分（这个在计划里）

GPL-3.0 开源，欢迎提 Issue、提 PR，尤其是复习算法和移动端体验。
