#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
${CXX:-clang++} -std=c++17 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
    source/game.cpp tests/test_game.cpp -o build-host/test-game
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build-host/test-game
