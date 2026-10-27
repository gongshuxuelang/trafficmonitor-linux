#!/usr/bin/env bash
# 生成在线包和离线包：
#   trafficmonitor-linux          在线包，Qt/glibc 依赖由 apt 从软件源安装
#   trafficmonitor-linux-offline  离线包，捆绑 Qt 运行库、Qt 插件及其依赖
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
用法: packaging/build-deb.sh [--skip-build]

环境变量:
  TM_BUILD_DIR    Release 构建目录，默认 build/release
  TM_PACKAGE_DIR  输出目录，默认 build/packages
  TM_DEB_RELEASE  Debian revision，默认取 CMakeLists.txt 中的 CPACK_DEBIAN_PACKAGE_RELEASE
  TM_OS_TAG       系统标识，默认从 /etc/os-release 生成，例如 ubuntu26
EOF
}

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "未知参数: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

for tool in dpkg-deb ldd; do
    command -v "$tool" >/dev/null 2>&1 || { echo "缺少命令: $tool" >&2; exit 1; }
done

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

# 系统标识：ubuntu26、debian13 之类，用于区分不同发行版的包
OS_TAG="${TM_OS_TAG:-}"
if [ -z "$OS_TAG" ] && [ -r /etc/os-release ]; then
    # 在子 shell 里读取，避免 /etc/os-release 的 VERSION 覆盖项目版本号
    OS_TAG=$(
        # shellcheck disable=SC1091
        . /etc/os-release
        printf '%s%s' "${ID:-}" "${VERSION_ID:-}"
    )
    OS_TAG="${OS_TAG%%.*}"
fi
OS_TAG=$(printf '%s' "${OS_TAG:-unknown}" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9')
[ -n "$OS_TAG" ] || OS_TAG="unknown"

DEB_VERSION="${VERSION}-${RELEASE}"
ARCH=$(dpkg --print-architecture)
ONLINE_NAME="${OS_TAG}-trafficmonitor-linux_${DEB_VERSION}_${ARCH}.deb"
OFFLINE_NAME="${OS_TAG}-trafficmonitor-linux-offline_${DEB_VERSION}_${ARCH}.deb"

if [ "$SKIP_BUILD" -eq 0 ]; then
    (cd "$ROOT" && cmake --preset release)
    (cd "$ROOT" && cmake --build --preset release)
fi

mkdir -p "$OUT_DIR"

echo "==> 在线包"
(cd "$ROOT" && cpack --preset deb \
    -D "CPACK_DEBIAN_PACKAGE_RELEASE=${RELEASE}" \
    -D "CPACK_DEBIAN_FILE_NAME=${ONLINE_NAME}")
ONLINE_DEB="$BUILD_DIR/$ONLINE_NAME"
[ -f "$ONLINE_DEB" ] || { echo "未找到 $ONLINE_DEB" >&2; exit 1; }
cp -f "$ONLINE_DEB" "$OUT_DIR/"

echo "==> 离线包"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/tm-deb-offline.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT

DESTDIR="$STAGE" cmake --install "$BUILD_DIR" --prefix /usr

BUNDLE="$STAGE/usr/lib/trafficmonitor-linux"
LIB_DIR="$BUNDLE/lib"
tm_bundle_runtime "$STAGE"

# 包内所有 ELF 需要的最高 glibc 版本；glibc 本身不打进包
GLIBC_MIN=""
if command -v objdump >/dev/null 2>&1; then
    while IFS= read -r -d '' file; do
        version=$(objdump -T "$file" 2>/dev/null \
            | grep -oE 'GLIBC_[0-9]+\.[0-9.]+' \
            | sed 's/^GLIBC_//' | sort -V | tail -1 || true)
        if [ -n "$version" ]; then
            if [ -z "$GLIBC_MIN" ] \
                || [ "$(printf '%s\n%s\n' "$GLIBC_MIN" "$version" | sort -V | tail -1)" = "$version" ]; then
                GLIBC_MIN="$version"
            fi
        fi
    done < <(
        printf '%s\0' "$STAGE/usr/bin/trafficmonitor-linux" \
            "$BUNDLE/plugins/gpu_plugin.so"
        find "$LIB_DIR" -type f -name '*.so*' -print0
    )
fi

DEPENDS="libc6"
[ -n "$GLIBC_MIN" ] && DEPENDS="libc6 (>= $GLIBC_MIN)"

chmod -R go-w "$STAGE"
find "$STAGE" -type d -exec chmod 755 {} +

mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<EOF
Package: trafficmonitor-linux-offline
Version: ${DEB_VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: TrafficMonitor for Linux contributors
Depends: ${DEPENDS}
Conflicts: trafficmonitor-linux
Replaces: trafficmonitor-linux
Installed-Size: $(du -sk "$STAGE" | cut -f1)
Description: Always-on-top network, CPU, memory and GPU monitor (offline bundle)
 Qt Widgets system monitor with a bundled Qt runtime, floating window,
 tray icon, SQLite history and a stable C ABI plugin system.
EOF
chmod 755 "$STAGE/DEBIAN"
chmod 644 "$STAGE/DEBIAN/control"

OFFLINE_DEB="$OUT_DIR/$OFFLINE_NAME"
dpkg-deb --build --root-owner-group "$STAGE" "$OFFLINE_DEB"

echo
echo "输出:"
echo "  $OUT_DIR/$(basename "$ONLINE_DEB")"
echo "  $OFFLINE_DEB"
