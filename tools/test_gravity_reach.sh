#!/usr/bin/env bash
# tests/gravity_reach.cpp with the real point, disk and cube gravity sources (UBSan), then three mutants of the reach test that
# must fail it: no margin for rounding (the shared test's, the cube's), the disk's radius left out, one of the cube's axes left out.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
docker run --rm --pull=never --platform linux/arm64 --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    clang -std=c11 -O2 -DTARGET_PC=1 -DPORT_PLATFORM_CODE=1 -include platform/include/port/compat.h -Iplatform/include \
        -Idecomp/libs/RVL_SDK/include -c platform/src/sdk/mtx.c -o /tmp/mtx.o
    build() {
        local tree=$1 out=$2
        clang++ -std=c++20 -w -O2 -g -fshort-wchar -fsigned-char -fms-extensions -DTARGET_PC=1 -fno-strict-aliasing -fwrapv \
            -ffp-contract=on -include vita/include/vita_source_prelude.h -include platform/include/port/compat.h \
            -Iplatform/include -I"$tree/include" -Idecomp/libs/JSystem/include -Idecomp/libs/RVL_SDK/include -Idecomp/libs/nw4r/include \
            -Idecomp/libs/RVLFaceLib/include -Wno-register -Wno-c++11-narrowing -Wno-microsoft-template -fno-exceptions -fno-rtti \
            -fsanitize=undefined -fno-omit-frame-pointer \
            tests/gravity_reach.cpp "$tree"/src/Game/Gravity/{PlanetGravity,PointGravity,DiskGravity,CubeGravity}.cpp \
            decomp/src/Game/Util/MathUtil.cpp decomp/src/JSystem/JMath/JMATrigonometric.cpp decomp/src/JSystem/JMath/JMath.cpp /tmp/mtx.o \
            -Wl,--unresolved-symbols=ignore-all -lm -o "$out"
    }
    build decomp /tmp/gravity_reach
    UBSAN_OPTIONS=halt_on_error=1 /tmp/gravity_reach 20000

    mutant() {
        local name=$1 file=$2 expr=$3
        rm -rf /tmp/m && mkdir -p /tmp/m && cp -r decomp/include decomp/src /tmp/m/
        cp "/tmp/m/$file" /tmp/before
        sed -i -e "$expr" "/tmp/m/$file"
        if cmp -s /tmp/before "/tmp/m/$file"; then echo "mutant $name: nothing changed"; exit 1; fi
        build /tmp/m /tmp/gravity_mutant
        if /tmp/gravity_mutant 20000 > /tmp/mutant.log; then
            echo "mutant $name survived:"; cat /tmp/mutant.log; exit 1
        fi
        echo "mutant $name caught: $(tail -1 /tmp/mutant.log)"
    }
    mutant no-margin include/Game/Gravity/PlanetGravity.hpp "s/\* 1\.001f +/+ 0.0f */; s/\* 1e-5f + 1\.0f;/* 0.0f;/"
    mutant no-disk-radius src/Game/Gravity/DiskGravity.cpp "s/relativePos\.squared(), mWorldRadius,/relativePos.squared(), 0.0f,/"
    mutant cube-no-margin src/Game/Gravity/CubeGravity.cpp "s/mDistant) \* 1\.001f +/mDistant) + 0.0f */; s/^\( *\)1\.0f;$/\10.0f;/"
    mutant cube-axis src/Game/Gravity/CubeGravity.cpp "s/(lenX + lenY + lenZ + mRange/(lenX + lenY + mRange/"
    '
