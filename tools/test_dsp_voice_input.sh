#!/usr/bin/env bash
# tests/dsp_voice_input.cpp: the resampler and AFC decoder in every GXV_DSPRESAMPLE/GXV_DSPAFC mode, with and without
# NEON, match the original loops, and the whole mixer gives one digest in all of them.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
docker run --rm --pull=never --platform linux/arm64 --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    flags=(-std=c++20 -O2 -g -fwrapv -fms-extensions -DTARGET_PC=1 -DPORT_PLATFORM_CODE=1 -DPORT_RUNTIME_DIAGNOSTICS=0
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -Iplatform/include -Idecomp/include -Idecomp/libs/JSystem/include -Idecomp/libs/RVL_SDK/include
        -Idecomp/libs/nw4r/include -Idecomp/libs/RVLFaceLib/include
        -include platform/include/port/compat.h -Wno-register -Wno-unknown-pragmas)
    export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
    clang++ "${flags[@]}" tests/dsp_voice_input.cpp -o /tmp/voice-neon
    clang++ "${flags[@]}" -DPORT_DSPRESAMPLE_NO_NEON tests/dsp_voice_input.cpp -o /tmp/voice-scalar
    digests=()
    for bin in /tmp/voice-neon /tmp/voice-scalar; do
        for modes in "0 0" "1 1" "2 2" "1 0" "0 1" "2 1" "3 garbage"; do
            set -- $modes
            line=$(GXV_DSPRESAMPLE=$1 GXV_DSPAFC=$2 "$bin")
            echo "$(basename "$bin") $line"
            digests+=("$(echo "$line" | cut -d" " -f2)")
        done
    done
    test "$(printf "%s\n" "${digests[@]}" | sort -u | wc -l)" -eq 1
    echo "PASS: one mixer digest in every mode, with and without NEON; resampler and AFC decoder exact"
    '
