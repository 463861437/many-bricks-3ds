#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/many-bricks-render.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"${HOST_CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Iinclude tests/test_render.cpp source/game.cpp source/render.cpp -o "$tmp/check-render"
"$tmp/check-render"
