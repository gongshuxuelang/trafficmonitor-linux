#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

export TRAFFICMONITOR_PLUGIN_DIR="${TRAFFICMONITOR_PLUGIN_DIR:-$ROOT/build/plugins}"
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] && [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=xcb
fi

exec "$ROOT/build/trafficmonitor-linux" "$@"
