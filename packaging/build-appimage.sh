#!/usr/bin/env bash
# 生成通用 AppImage：捆绑 Qt 运行库、Qt 插件和 TrafficMonitor 插件。
# 在 Ubuntu 22.04 上构建时 glibc 下限是 2.35，可运行在大多数当前发行版上。
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=bundle-runtime.sh
. "$SCRIPT_DIR/bundle-runtime.sh"
BUILD_DIR="${TM_BUILD_DIR:-$ROOT/build/release}"
OUT_DIR="${TM_PACKAGE_DIR:-$ROOT/build/packages}"
SKIP_BUILD=0

usage() {
    cat <<'EOF'
用法: packaging/build-appimage.sh [--skip-build]

环境变量:
  TM_BUILD_DIR    Release 构建目录，默认 build/release
  TM_PACKAGE_DIR  输出目录，默认 build/packages
  TM_DEB_RELEASE  版本后缀，默认取 CMakeLists.txt 中的 CPACK_DEBIAN_PACKAGE_RELEASE
EOF
}

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "未知参数: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

for tool in ldd curl; do
    command -v "$tool" >/dev/null 2>&1 || { echo "缺少命令: $tool" >&2; exit 1; }
done
command -v mksquashfs >/dev/null 2>&1 \
    || { echo "缺少 mksquashfs，请安装 squashfs-tools" >&2; exit 1; }

VERSION=$(sed -n 's/^project(trafficmonitor_linux VERSION \([0-9][0-9.]*\).*/\1/p' \
    "$ROOT/CMakeLists.txt" | head -1)
DEFAULT_RELEASE=$(sed -n 's/^set(CPACK_DEBIAN_PACKAGE_RELEASE "\([^"]*\)".*/\1/p' \
    "$ROOT/CMakeLists.txt" | head -1)
RELEASE="${TM_DEB_RELEASE:-${DEFAULT_RELEASE:-1}}"
[ -n "$VERSION" ] || { echo "无法从 CMakeLists.txt 读取版本号" >&2; exit 1; }
case "$RELEASE" in
    *[!A-Za-z0-9.+~]*)
        echo "TM_DEB_RELEASE 只能包含字母、数字和 . + ~，当前值: $RELEASE" >&2
        exit 1
        ;;
esac

ARCH=$(uname -m)
APPIMAGE_NAME="trafficmonitor-linux_${VERSION}-${RELEASE}_${ARCH}.AppImage"

if [ "$SKIP_BUILD" -eq 0 ]; then
    (cd "$ROOT" && cmake --preset release)
    (cd "$ROOT" && cmake --build --preset release)
fi

mkdir -p "$OUT_DIR"
APPDIR=$(mktemp -d "${TMPDIR:-/tmp}/tm-appimage.XXXXXX")
trap 'rm -rf "$APPDIR"' EXIT

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr
rm -f "$APPDIR/usr/bin/trafficmonitor-linux-wrapper"
rm -f "$APPDIR/usr/share/applications/trafficmonitor-linux.desktop"

tm_bundle_runtime "$APPDIR"

cp "$ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"
chmod 755 "$APPDIR/AppRun"
cp "$ROOT/packaging/appimage/trafficmonitor-linux.desktop" "$APPDIR/"
cp "$ROOT/packaging/icons/trafficmonitor-linux.png" "$APPDIR/trafficmonitor-linux.png"
cp "$ROOT/packaging/icons/trafficmonitor-linux.png" "$APPDIR/.DirIcon"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$ROOT/packaging/icons/trafficmonitor-linux.png" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps/"

TOOL_DIR="$ROOT/build/tools"
TOOL="${TM_APPIMAGETOOL:-$TOOL_DIR/appimagetool-${ARCH}.AppImage}"
mkdir -p "$TOOL_DIR"
if [ ! -x "$TOOL" ]; then
    echo "下载 appimagetool..."
    curl -fsSL --retry 3 --retry-all-errors -o "$TOOL" \
        "https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-${ARCH}.AppImage"
    chmod 755 "$TOOL"
fi

ARCH="$ARCH" APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" "$APPDIR" "$OUT_DIR/$APPIMAGE_NAME"

echo
echo "输出:"
echo "  $OUT_DIR/$APPIMAGE_NAME"
