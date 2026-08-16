# 鸿蒙（HarmonyOS）适配说明

## 结论

代码层面已“鸿蒙就绪”：移动端（`mobile/`）是 Qt Quick/QML + 纯 C++ core，
只用 Qt 6 标准 API，不依赖任何 Android/iOS 专属接口；QML 页面、桥接层、
核心逻辑均可在 Qt for HarmonyOS/OpenHarmony 上原样编译。

当前真正缺的不是代码，而是**工具链、签名与设备**：

- DevEco Studio（含 OpenHarmony SDK / NDK）
- 华为开发者账号（真机调试与发布签名，无证书无法安装到鸿蒙 NEXT 真机）
- 一台鸿蒙 NEXT 设备或模拟器
- 可用的 Qt for HarmonyOS 构建（见下）

## 两条技术路线

### 路线 A：Qt 官方支持（推荐，等待正式版）

Qt 官方从 6.12（Beta）开始提供 HarmonyOS 支持：

- 要求 DevEco Studio + OpenHarmony SDK API 20+（Qt 官方 wiki 提到 API 23）
- 参考：<https://wiki.qt.io/Qt_for_HarmonyOS_development_with_6.12.0_Beta2>
- 正式版发布后，本仓库只需在 CI 增加一个鸿蒙构建 job（类似现有 android job），
  使用 Qt 官方 kit 交叉编译 `english3000_mobile`，再用 DevEco/hvigor 打包 HAP

### 路线 B：社区 Qt for OpenHarmony（现在就能试）

OpenHarmony SIG 维护的 Qt 移植（qtforohos）：

- Qt 6.5.6 / 5.15.17 适配 API 20，支持 OpenHarmony 6.0 / HarmonyOS NEXT 5.1+
- 仓库：<https://gitcode.com/openharmony-sig/qt>
- 需要先按该仓库脚本在 Linux 上编译 Qt for OpenHarmony（约 1~2 小时），
  然后使用其 toolchain 交叉编译本项目

## 构建步骤（路线 B，概述）

```bash
# 1. 安装 DevEco Studio，从其 SDK Manager 下载 native/OpenHarmony SDK
# 2. 克隆社区 Qt 移植并按文档编译（qtbase 等必要模块）
# 3. 交叉编译本项目移动端
cmake -B build-ohos \
  -DCMAKE_TOOLCHAIN_FILE=<qt-for-ohos>/toolchain.cmake \
  -DOHOS_SDK=<dev-eco-sdk>/openharmony \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-ohos --target english3000_mobile
# 4. 用 hvigor / DevEco 打包成 HAP，并用华为证书签名
```

## 本仓库已做的准备

- `mobile/` 全部 QML 页面（学习/阅读/词表/翻译/设置）均为标准 Qt Quick
- `MobileBridge` 只依赖 Qt Core/Network/Quick，TTS 用 `ENGLISH3000_HAS_TTS`
  条件编译，缺模块也能编译
- CMake 工程与平台无关（同一份源码覆盖 Linux/Windows/Android/iOS/OHOS）

## 待办

- 等待 Qt 6.12 正式版对 HarmonyOS 的支持稳定
- 用户提供鸿蒙设备 / 华为开发者账号 / 签名配置后，在 CI 增加鸿蒙构建 job
- 移动端数据统计页补齐（与桌面一致）
