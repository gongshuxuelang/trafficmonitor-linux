#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

BUILD_DIR="${TRAFFICMONITOR_BUILD_DIR:-}"
if [ -z "$BUILD_DIR" ]; then
    if [ -x "$ROOT/build/debug/trafficmonitor-linux" ]; then
        BUILD_DIR="$ROOT/build/debug"
    else
        BUILD_DIR="$ROOT/build"
    fi
fi

export TRAFFICMONITOR_PLUGIN_DIR="${TRAFFICMONITOR_PLUGIN_DIR:-$BUILD_DIR/plugins}"
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] && [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=xcb
fi

exec "$BUILD_DIR/trafficmonitor-linux" "$@"
