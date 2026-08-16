#!/usr/bin/env bash
# 组装桌面版 Qt 6.7.3 (linux gcc_64) 作为 QT_HOST_PATH
# 用法: ./tools/install-qt-host.sh <输出目录>
# 输出: <输出目录>/6.7.3/gcc_64
set -euo pipefail

QT_VERSION="6.7.3"
QT_SHORT="673"
REPO="https://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt6_${QT_SHORT}"
OUT="${1:?用法: install-qt-host.sh <输出目录>}"
PACKAGES=("qt.qt6.673.linux_gcc_64" "qt.qt6.673.qtshadertools.linux_gcc_64")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

command -v 7z >/dev/null || { echo "需要 7z (p7zip)"; exit 1; }
command -v python3 >/dev/null || { echo "需要 python3"; exit 1; }

echo "==> 解析 $REPO/Updates.xml"
python3 - "$REPO" "$TMP" "${PACKAGES[@]}" <<'PYEOF'
import sys, os, urllib.request, urllib.parse, xml.etree.ElementTree as ET

repo, tmp = sys.argv[1], sys.argv[2]
wanted = set(sys.argv[3:])
def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=120).read()

xml = fetch(repo + "/Updates.xml")
root = ET.fromstring(xml)
jobs = []
for p in root.iter("PackageUpdate"):
    name = p.findtext("Name") or ""
    if name not in wanted:
        continue
    ver = p.findtext("Version") or ""
    for a in (p.findtext("DownloadableArchives") or "").split(","):
        a = a.strip()
        if a:
            jobs.append((name, ver + a))
print(f"    共 {len(jobs)} 个归档, 开始下载...")
for name, fn in jobs:
    url = repo + "/" + urllib.parse.quote(name) + "/" + urllib.parse.quote(fn)
    dest = os.path.join(tmp, fn)
    data = fetch(url)
    open(dest, "wb").write(data)
    print(f"    {fn[:58]} ({len(data)//1024} KiB)")
PYEOF

echo "==> 解压到 $OUT"
mkdir -p "$OUT"
for f in "$TMP"/*.7z; do
    7z x -y -o"$OUT" "$f" >/dev/null
done
QTDIR="$OUT/$QT_VERSION/gcc_64"
if [ -f "$QTDIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "==> OK: $QTDIR"
else
    echo "==> 错误: 未找到 $QTDIR/lib/cmake/Qt6/Qt6Config.cmake"
    exit 1
fi
