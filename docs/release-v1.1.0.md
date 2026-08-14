# English 3000 v1.1.0

首个正式发布版，从 v1.0.0 以来完成了一次产品重构，并补齐了分发与更新体系。

## 下载

- Windows 一键安装包（内置本地 AI 小模型，推荐新手）：
  https://github.com/lianghanbin/english3000/releases/download/win-oneclick/English3000-OneClick-Setup.exe
- Windows 绿色版：
  https://github.com/lianghanbin/english3000/releases/download/win-latest/english3000-windows.zip
- Android APK：
  https://github.com/lianghanbin/english3000/releases/download/android-latest/english3000-mobile.apk

## 主要更新

### 学习模型重构

- 词表即学习对象：选中哪个词表就学哪个，进度独立记录
- 学习 / 复习分离：学习=没学过的词，点“不认识”进入复习队列
- 词表可拖拽排序，核心 3000 默认置顶

### AI 能力

- 支持 OpenAI 兼容云端 API（DeepSeek / 通义 / GLM / Kimi 等）
- 默认本地小模型 qwen2.5:1.5b
- Windows 一键包内置 llama.cpp + 1.5B 模型，CUDA → Vulkan → CPU 自动回退
- AI 词表生成过滤噪音词，释义空缺显示“待补充”

### 阅读

- 四色高亮：红=未入词表 / 蓝=其他词表 / 绿=当前词表 / 黑=已掌握
- 左键点词发音，选中一段右键直接朗读
- 右键菜单按颜色区分：红=加入阅读词表 / 蓝绿=所在词表 / 黑=重置

### 翻译与词表联动

- 全局热键翻译 + 截图翻译
- 翻译生词自动收集到「翻译生词」词表，阅读生词收集到「阅读生词」词表

### 体验

- 快捷键全部可自定义（学习 / 复习 / 显示释义 / 认识 / 不认识）
- 应用内自动更新：检查 → 下载 → SHA256 校验 → 自动安装 → 重启
- Android 端新增阅读页与词表页，APK 自动发布

### 开源

- 项目以 GPL-3.0 协议开源
