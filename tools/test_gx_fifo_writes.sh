#!/usr/bin/env bash
# tests/gx_fifo_writes.cpp: the FIFO's one-check writers (port/gx_fifo.h, GXVert.h) against field-by-field writes, with ASan/UBSan.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
docker run --rm --pull=never --platform linux/arm64 --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    flags=(-O2 -g -fms-extensions -DTARGET_PC=1 -fno-strict-aliasing -ffp-contract=on -fno-fast-math
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -Iplatform/include -Idecomp/libs/RVL_SDK/include -Idecomp/include)
    clang++ -std=c++20 "${flags[@]}" -Wno-unused-function tests/gx_fifo_writes.cpp -o /tmp/gx_fifo_writes
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/gx_fifo_writes
    '
