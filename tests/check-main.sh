#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
flags="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
"${HOST_CXX:-c++}" $flags -Itests/main-host -Iinclude -Dmain=bricksProgramMain \
    -c source/main.cpp -o build-host/main-loop.o
"${HOST_CXX:-c++}" $flags -Itests/main-host -Iinclude \
    source/game.cpp source/controls.cpp tests/main-host/test_main.cpp build-host/main-loop.o \
    -o build-host/test-main
for scenario in keys touch home sleep dt quit save; do
    ./build-host/test-main "$scenario"
done
