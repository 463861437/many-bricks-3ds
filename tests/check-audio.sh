#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
"${HOST_CXX:-c++}" -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Itests/audio-host/include -Iinclude \
    source/audio.cpp tests/test_audio.cpp -o build-host/test-audio
./build-host/test-audio
