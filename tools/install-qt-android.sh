#!/usr/bin/env bash
# 手动组装 Qt Android 工具链(绕过 aqtinstall 对新 CDN 结构的兼容问题)
# 用法: ./tools/install-qt-android.sh <输出目录>
# 输出: <输出目录>/6.7.3/android_arm64_v8a 与 .../android_x86_64
set -euo pipefail

QT_VERSION="6.7.3"
QT_SHORT="673"
BASE="https://download.qt.io/online/qtsdkrepository/all_os/android"
OUT="${1:?用法: install-qt-android.sh <输出目录>}"
ARCHES=(android_arm64_v8a android_x86_64)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

command -v 7z >/dev/null || { echo "需要 7z (p7zip)"; exit 1; }
command -v python3 >/dev/null || { echo "需要 python3"; exit 1; }

for ARCH in "${ARCHES[@]}"; do
    # CDN 仓库目录无 android_ 前缀(qt6_673_arm64_v8a),7z 内部才是 6.7.3/android_arm64_v8a/
    REPO_DIR="qt6_${QT_SHORT}_${ARCH#android_}"
    REPO="$BASE/$REPO_DIR"
    echo "==> 解析 $REPO/Updates.xml"
    python3 - "$REPO" "$TMP" <<'PYEOF'
import sys, os, urllib.request, urllib.parse, xml.etree.ElementTree as ET

repo, tmp = sys.argv[1], sys.argv[2]
def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=120).read()

xml = fetch(repo + "/Updates.xml")
root = ET.fromstring(xml)
jobs = []
for p in root.iter("PackageUpdate"):
    name = p.findtext("Name") or ""
    ver = p.findtext("Version") or ""
    for a in (p.findtext("DownloadableArchives") or "").split(","):
        a = a.strip()
        if a:
            jobs.append((name, ver + a))
print(f"    共 {len(jobs)} 个归档, 开始下载...")
for name, fn in jobs:
    url = repo + "/" + urllib.parse.quote(name) + "/" + urllib.parse.quote(fn)
    dest = os.path.join(tmp, fn)
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        continue
    try:
        data = fetch(url)
        open(dest, "wb").write(data)
        print(f"    {fn[:60]} ({len(data)//1024} KiB)")
    except Exception as e:
        print(f"    FAIL {fn[:60]}: {e}")
        sys.exit(1)
PYEOF
    echo "==> 解压 $ARCH 到 $OUT"
    mkdir -p "$OUT"
    for f in "$TMP"/*.7z; do
        7z x -y -o"$OUT" "$f" >/dev/null
    done
    TC="$OUT/$QT_VERSION/$ARCH/lib/cmake/Qt6/qt.toolchain.cmake"
    if [ -f "$TC" ]; then
        echo "==> OK: $TC"
    else
        echo "==> 错误: 未找到工具链文件 $TC"
        exit 1
    fi
done
echo "完成: Qt $QT_VERSION android 工具链位于 $OUT"
