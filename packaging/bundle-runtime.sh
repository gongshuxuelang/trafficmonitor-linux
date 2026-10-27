#!/usr/bin/env bash
# 把 Qt 运行库、平台插件和它们的依赖复制到
# <stage>/usr/lib/trafficmonitor-linux/{lib,qt-plugins}。
# 由 build-deb.sh、build-appimage.sh、build-rpm.sh、build-arch.sh 复用。
#
# 用法:
#   . packaging/bundle-runtime.sh
#   tm_bundle_runtime <stage-dir>

declare -gA TM_BUNDLE_LIBS=()

tm_bundle_is_system_lib() {
    case "$1" in
        ld-linux*|linux-vdso*|libc.so.*|libm.so.*|libmvec.so.*|libdl.so.*|\
        libpthread.so.*|librt.so.*|libresolv.so.*|libutil.so.*|libanl.so.*)
            return 0
            ;;
        *) return 1 ;;
    esac
}

tm_bundle_collect() {
    local file=$1 lib base
    while IFS= read -r lib; do
        [ -n "$lib" ] || continue
        base=$(basename "$lib")
        tm_bundle_is_system_lib "$base" && continue
        [ -n "${TM_BUNDLE_LIBS[$base]:-}" ] && continue
        TM_BUNDLE_LIBS[$base]="$lib"
    done < <(ldd "$file" 2>/dev/null | awk '/=> \// { print $3 } /^[[:space:]]*\// { print $1 }')
}

tm_bundle_runtime() {
    local stage=$1
    local bundle="$stage/usr/lib/trafficmonitor-linux"
    local lib_dir="$bundle/lib"
    local qt_plugin_dir="$bundle/qt-plugins"
    local qt_plugin_root qt_core subdir base libfile added

    mkdir -p "$lib_dir" "$qt_plugin_dir"

    qt_plugin_root=$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)
    if [ -z "$qt_plugin_root" ] || [ ! -d "$qt_plugin_root" ]; then
        qt_plugin_root=$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || true)
    fi
    if [ -z "$qt_plugin_root" ] || [ ! -d "$qt_plugin_root" ]; then
        # 回退：从 libQt6Core 的位置推导插件目录
        qt_core=$(ldd "$stage/usr/bin/trafficmonitor-linux" 2>/dev/null \
            | awk '/libQt6Core\.so/ { print $3; exit }')
        if [ -n "$qt_core" ]; then
            qt_plugin_root="$(dirname "$qt_core")/qt6/plugins"
        fi
    fi
    if [ -z "$qt_plugin_root" ] || [ ! -d "$qt_plugin_root" ]; then
        echo "找不到 Qt 插件目录，需要 qmake6、qtpaths6 或可用的 Qt6 安装" >&2
        return 1
    fi

    # 平台插件（xcb/offscreen/wayland）、SQLite 驱动、图片和图标格式、xcb GL 集成
    for subdir in platforms sqldrivers imageformats iconengines xcbglintegrations; do
        if [ -d "$qt_plugin_root/$subdir" ]; then
            cp -aL "$qt_plugin_root/$subdir" "$qt_plugin_dir/"
        fi
    done

    TM_BUNDLE_LIBS=()
    tm_bundle_collect "$stage/usr/bin/trafficmonitor-linux"
    tm_bundle_collect "$bundle/plugins/gpu_plugin.so"
    while IFS= read -r -d '' libfile; do
        tm_bundle_collect "$libfile"
    done < <(find "$qt_plugin_dir" -type f -name '*.so' -print0)

    # 复制库，并对新复制的库继续收集，直到闭包不再增长
    while :; do
        added=0
        for base in "${!TM_BUNDLE_LIBS[@]}"; do
            if [ ! -f "$lib_dir/$base" ]; then
                cp -L "${TM_BUNDLE_LIBS[$base]}" "$lib_dir/$base"
                added=1
            fi
        done
        [ "$added" -eq 0 ] && break
        while IFS= read -r -d '' libfile; do
            tm_bundle_collect "$libfile"
        done < <(find "$lib_dir" -type f -name '*.so*' -print0)
    done
}
