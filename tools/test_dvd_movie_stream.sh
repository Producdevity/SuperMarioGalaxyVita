#!/usr/bin/env bash
# The movie reader's host test (tests/dvd_movie_stream.cpp), under the address and thread sanitizers.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$(mktemp -d)}
mkdir -p "$out"
cxx=${CXX:-clang++}
"$cxx" -std=c++17 -O1 -g -Wall -Wextra -fsanitize=address,undefined "$root/tests/dvd_movie_stream.cpp" -pthread -o "$out/asan"
"$out/asan"
"$cxx" -std=c++17 -O1 -g -Wall -Wextra -fsanitize=thread "$root/tests/dvd_movie_stream.cpp" -pthread -o "$out/tsan"
"$out/tsan"
