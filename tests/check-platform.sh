#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
"${HOST_CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
    source/game.cpp source/controls.cpp source/save.cpp tests/test_platform.cpp -o build-host/test-platform
./build-host/test-platform
