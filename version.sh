#!/usr/bin/env sh

set -eu

DIR=$(dirname "$0")

VER_FILE="${DIR}/.version"

touch -c "${VER_FILE}"

if [ -d "$DIR/.git" ]; then
    EXACT_TAG=$(git name-rev --name-only --tags HEAD)
    if [ "$EXACT_TAG" = 'undefined' ]; then
        HEAD=$(git rev-parse --short HEAD)
        DATE=$(git log -1 --format=%cd --date=format:'%Y-%m-%d')
        VER="${HEAD}"-"${DATE}"
    else
        VER="$EXACT_TAG"
    fi
else
    VER="$(head -1 "${VER_FILE}")"
fi

echo "$VER"
