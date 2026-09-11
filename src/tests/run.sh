#!/bin/bash

set -euo pipefail

# Simple DBUS session wrapper to not change user dconf keys

CMD="$@"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/config/dconf"
mkdir -p "$tmp/data"

# A private dconf profile.
cat >"$tmp/dconf-profile" <<EOF
user-db:test
EOF

dbus-run-session -- env \
    DCONF_PROFILE="$tmp/dconf-profile" \
    XDG_CONFIG_HOME="$tmp/config" \
    XDG_DATA_HOME="$tmp/data" \
    GSETTINGS_BACKEND=dconf \
    GSETTINGS_SCHEMA_DIR="$(dirname "$0")/../../data" \
    $CMD
