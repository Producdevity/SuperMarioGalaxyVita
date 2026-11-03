#!/usr/bin/env bash
# tests/vgx_sharpen_fast.cpp: the present's clamped and fast sharpening programs, with their sources taken from
# platform/vgx/src/vgx_backend_gl.cpp, on Mesa (EGL, no surface).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
image=${GALAXY_TEST_IMAGE:-codex-galaxy-native:20261007-arm64}
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
python3 -I - "$root/platform/vgx/src/vgx_backend_gl.cpp" "$out/sharpen_sources.h" <<'EOF'
import re, sys
src = open(sys.argv[1]).read()
def sources(function):
    body = src[src.index("bool " + function + "() {"):]
    body = body[:body.index("\n}\n")]
    found = {}
    for name in ("vs", "fs"):
        m = re.search(r'static const char\* ' + name + r' =((?:\s*"(?:[^"\\]|\\.)*")+);', body)
        if not m:
            sys.exit("no %s in %s" % (name, function))
        found[name] = m.group(1)
    return found
c, f = sources("EnsureSharpenProgram"), sources("EnsureSharpenFastProgram")
with open(sys.argv[2], "w") as h:
    h.write("static const char* kClampedVs =%s;\nstatic const char* kClampedFs =%s;\n" % (c["vs"], c["fs"]))
    h.write("static const char* kFastVs =%s;\nstatic const char* kFastFs =%s;\n" % (f["vs"], f["fs"]))
EOF
docker run --rm --pull=never --network none --cpus=2 \
    --mount "type=bind,source=$root,target=/work,readonly" --mount "type=bind,source=$out,target=/gen" -w /work \
    --entrypoint bash "$image" -euo pipefail -c '
    clang++ -std=c++17 -O1 -g -I/gen tests/vgx_sharpen_fast.cpp -lEGL -lGLESv2 -o /tmp/vgx_sharpen_fast
    EGL_PLATFORM=surfaceless /tmp/vgx_sharpen_fast
    '
