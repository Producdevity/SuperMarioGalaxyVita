#!/usr/bin/env bash
# tests/os_yield.cpp (includes platform/src/os/os_thread.cpp), ASan/UBSan.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
docker run --rm --pull=never --platform linux/arm64 --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    clang++ -std=c++20 -O1 -g -DTARGET_PC=1 -DNDEBUG=1 -fms-extensions -ffunction-sections -Wl,--gc-sections \
        -Iplatform/include -Idecomp/libs/RVL_SDK/include -include platform/include/port/compat.h \
        -Wno-register -Wno-unknown-pragmas -fsanitize=address,undefined -fno-omit-frame-pointer \
        tests/os_yield.cpp -lpthread -o /tmp/os_yield
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/os_yield
    clang++ -std=c++20 -O1 -g -DTARGET_PC=1 -DNDEBUG=1 -fms-extensions -ffunction-sections -Wl,--gc-sections \
        -Iplatform/include -Idecomp/libs/RVL_SDK/include -include platform/include/port/compat.h \
        -Wno-register -Wno-unknown-pragmas -fsanitize=address,undefined -fno-omit-frame-pointer \
        tests/os_irq.cpp -lpthread -o /tmp/os_irq
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 /tmp/os_irq
    '
