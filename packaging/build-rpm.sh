#!/usr/bin/env bash
# 生成 RPM：
#   trafficmonitor-linux          在线包，Qt 依赖由 dnf/zypper 安装
#   trafficmonitor-linux-offline  离线包，捆绑 Qt 运行库、Qt 插件及其依赖
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# shellcheck source=bundle-runtime.sh
. "$SCRIPT_DIR/bundle-runtime.sh"
BUILD_DIR="${TM_BUILD_DIR:-$ROOT/build/release}"
OUT_DIR="${TM_PACKAGE_DIR:-$ROOT/build/packages}"
SKIP_BUILD=0
RPM_LICENSE="${TM_RPM_LICENSE:-Unspecified}"

usage() {
    cat <<'EOF'
用法: packaging/build-rpm.sh [--skip-build]

环境变量:
  TM_BUILD_DIR    Release 构建目录，默认 build/release
  TM_PACKAGE_DIR  输出目录，默认 build/packages
  TM_DEB_RELEASE  版本后缀，默认取 CMakeLists.txt 中的 CPACK_DEBIAN_PACKAGE_RELEASE
  TM_RPM_DIST     发行版标识，默认从 /etc/os-release 生成，例如 fc41、el9、suse15
  TM_RPM_LICENSE  License 字段，默认 Unspecified
EOF
}

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "未知参数: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

for tool in rpmbuild ldd; do
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

DIST_TAG="${TM_RPM_DIST:-}"
if [ -z "$DIST_TAG" ] && [ -r /etc/os-release ]; then
    DIST_TAG=$(
        # shellcheck disable=SC1091
        . /etc/os-release
        case "${ID:-}" in
            fedora) printf 'fc%s' "${VERSION_ID:-}" ;;
            rhel|centos|rocky|almalinux) printf 'el%s' "${VERSION_ID%%.*}" ;;
            opensuse*|sles) printf 'suse%s' "${VERSION_ID%%.*}" ;;
            *) printf '%s' "${ID:-}" ;;
        esac
    )
fi
DIST_TAG=$(printf '%s' "${DIST_TAG:-local}" | tr -cd 'a-zA-Z0-9')
[ -n "$DIST_TAG" ] || DIST_TAG="local"

RPM_RELEASE="${RELEASE}.${DIST_TAG}"
RPM_ARCH=$(uname -m)
ONLINE_NAME="trafficmonitor-linux-${VERSION}-${RPM_RELEASE}.${RPM_ARCH}.rpm"
OFFLINE_NAME="trafficmonitor-linux-offline-${VERSION}-${RPM_RELEASE}.${RPM_ARCH}.rpm"

if [ "$SKIP_BUILD" -eq 0 ]; then
    (cd "$ROOT" && cmake --preset release)
    (cd "$ROOT" && cmake --build --preset release)
fi

mkdir -p "$OUT_DIR"

echo "==> 在线包"
(cd "$BUILD_DIR" && cpack -G RPM \
    -D "CPACK_RPM_PACKAGE_RELEASE=${RPM_RELEASE}" \
    -D "CPACK_RPM_PACKAGE_LICENSE=${RPM_LICENSE}" \
    -D "CPACK_RPM_FILE_NAME=${ONLINE_NAME}")
ONLINE_RPM="$BUILD_DIR/$ONLINE_NAME"
[ -f "$ONLINE_RPM" ] || { echo "未找到 $ONLINE_RPM" >&2; exit 1; }
cp -f "$ONLINE_RPM" "$OUT_DIR/"

echo "==> 离线包"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/tm-rpm-offline.XXXXXX")
TOP=$(mktemp -d "${TMPDIR:-/tmp}/tm-rpmbuild.XXXXXX")
trap 'rm -rf "$STAGE" "$TOP"' EXIT

DESTDIR="$STAGE" cmake --install "$BUILD_DIR" --prefix /usr
tm_bundle_runtime "$STAGE"
chmod -R go-w "$STAGE"
find "$STAGE" -type d -exec chmod 755 {} +

mkdir -p "$TOP/BUILD" "$TOP/BUILDROOT" "$TOP/RPMS" "$TOP/SOURCES" "$TOP/SPECS" "$TOP/SRPMS"
(cd "$STAGE" && find usr -type f) | sed 's#^#/#' > "$TOP/filelist"

cat > "$TOP/SPECS/trafficmonitor-linux-offline.spec" <<EOF
Name: trafficmonitor-linux-offline
Version: ${VERSION}
Release: ${RPM_RELEASE}
Summary: Always-on-top network, CPU, memory and GPU monitor (offline bundle)
License: ${RPM_LICENSE}
BuildArch: ${RPM_ARCH}
AutoReqProv: no
Requires: glibc
Conflicts: trafficmonitor-linux

%description
Qt Widgets system monitor with a bundled Qt runtime, floating window,
tray icon, SQLite history and a stable C ABI plugin system.

%install
mkdir -p %{buildroot}
cp -a "${STAGE}/." "%{buildroot}/"

%files -f ${TOP}/filelist
EOF

rpmbuild -bb \
    --define "_topdir $TOP" \
    --define "_rpmdir $TOP/RPMS" \
    --define "_build_id_links none" \
    "$TOP/SPECS/trafficmonitor-linux-offline.spec"

OFFLINE_RPM=$(find "$TOP/RPMS" -name '*.rpm' -print -quit)
[ -n "$OFFLINE_RPM" ] || { echo "rpmbuild 未生成 RPM" >&2; exit 1; }
cp -f "$OFFLINE_RPM" "$OUT_DIR/$OFFLINE_NAME"

echo
echo "输出:"
echo "  $OUT_DIR/$ONLINE_NAME"
echo "  $OUT_DIR/$OFFLINE_NAME"
