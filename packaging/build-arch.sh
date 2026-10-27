#!/usr/bin/env bash
# 生成 Arch 包（.pkg.tar.zst）：
#   trafficmonitor-linux          在线包，依赖 qt6-base、qt6-svg
#   trafficmonitor-linux-offline  离线包，捆绑 Qt 运行库和插件
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=bundle-runtime.sh
. "$SCRIPT_DIR/bundle-runtime.sh"
BUILD_DIR="${TM_BUILD_DIR:-$ROOT/build/release}"
OUT_DIR="${TM_PACKAGE_DIR:-$ROOT/build/packages}"
SKIP_BUILD=0
ARCH_LICENSE="${TM_ARCH_LICENSE:-custom}"

usage() {
    cat <<'EOF'
用法: packaging/build-arch.sh [--skip-build]

环境变量:
  TM_BUILD_DIR    Release 构建目录，默认 build/release
  TM_PACKAGE_DIR  输出目录，默认 build/packages
  TM_DEB_RELEASE  版本后缀，默认取 CMakeLists.txt 中的 CPACK_DEBIAN_PACKAGE_RELEASE
  TM_ARCH_LICENSE license 字段，默认 custom
EOF
}

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "未知参数: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

for tool in makepkg ldd; do
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

PKGVER="${VERSION}.${RELEASE}"
PKGREL=1
PKG_ARCH=$(uname -m)

if [ "$SKIP_BUILD" -eq 0 ]; then
    (cd "$ROOT" && cmake --preset release)
    (cd "$ROOT" && cmake --build --preset release)
fi

mkdir -p "$OUT_DIR"

write_pkgbuild() {
    local name=$1 stage=$2 depends=$3 conflicts=$4 work=$5
    cat > "$work/PKGBUILD" <<EOF
pkgname=${name}
pkgver=${PKGVER}
pkgrel=${PKGREL}
pkgdesc="Always-on-top network, CPU, memory and GPU monitor"
arch=('${PKG_ARCH}')
url=""
license=('${ARCH_LICENSE}')
depends=(${depends})
conflicts=(${conflicts})
options=('!emptydirs')

package() {
    cp -a "${stage}/." "\$pkgdir/"
}
EOF
}

run_makepkg() {
    local work=$1
    if [ "$(id -u)" -eq 0 ]; then
        id -u builder >/dev/null 2>&1 || useradd -m -s /bin/bash builder
        chown -R builder:builder "$work"
        su -s /bin/bash builder -c \
            "cd '$work' && PATH=/usr/bin:/bin makepkg -f --noconfirm --nodeps"
    else
        (cd "$work" && makepkg -f --noconfirm --nodeps)
    fi
}

build_package() {
    local name=$1 depends=$2 conflicts=$3 bundle_runtime=$4
    local stage work pkg
    stage=$(mktemp -d "${TMPDIR:-/tmp}/tm-arch-stage.XXXXXX")
    work=$(mktemp -d "${TMPDIR:-/tmp}/tm-arch-build.XXXXXX")

    DESTDIR="$stage" cmake --install "$BUILD_DIR" --prefix /usr
    if [ "$bundle_runtime" = "1" ]; then
        tm_bundle_runtime "$stage"
        chmod -R go-w "$stage"
        find "$stage" -type d -exec chmod 755 {} +
    fi

    write_pkgbuild "$name" "$stage" "$depends" "$conflicts" "$work"
    run_makepkg "$work"

    pkg=$(find "$work" -maxdepth 1 -name '*.pkg.tar.zst' -print -quit)
    [ -n "$pkg" ] || { echo "makepkg 未生成 $name" >&2; exit 1; }
    cp -f "$pkg" "$OUT_DIR/"
    rm -rf "$stage" "$work"
}

echo "==> 在线包"
build_package "trafficmonitor-linux" "'qt6-base' 'qt6-svg'" "" 0

echo "==> 离线包"
build_package "trafficmonitor-linux-offline" "'glibc'" "'trafficmonitor-linux'" 1

echo
echo "输出:"
find "$OUT_DIR" -maxdepth 1 -name "trafficmonitor-linux-${PKGVER}-${PKGREL}-*.pkg.tar.zst" \
    -o -name "trafficmonitor-linux-offline-${PKGVER}-${PKGREL}-*.pkg.tar.zst" | sort
