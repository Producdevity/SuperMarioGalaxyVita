#!/usr/bin/env bash
# tests/os_region_lookup.cpp with platform/src/os/os_mem.cpp: the desktop banks (with uncached aliases) and the rebased
# ones (PETARI_NATIVE_ADDRESSES=1, no aliases), ASan/UBSan.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
docker run --rm --pull=never --platform linux/arm64 --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    clang++ -std=c++20 -O2 -g -fms-extensions -DTARGET_PC=1 -DPORT_PLATFORM_CODE=1 \
        -include platform/include/port/compat.h -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
        -Wno-unknown-pragmas -Wno-register -Iplatform/include -Idecomp/libs/RVL_SDK/include \
        -isystem decomp/libs/JSystem/include -isystem decomp/include \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -mcmodel=large -fno-pie -no-pie -fuse-ld=lld -Wl,--image-base=0xB8000000 \
        tests/os_region_lookup.cpp platform/src/os/os_mem.cpp -lpthread -o /tmp/os_region_lookup
    export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
    /tmp/os_region_lookup
    PETARI_NATIVE_ADDRESSES=1 /tmp/os_region_lookup
    '
