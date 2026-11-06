#!/usr/bin/env bash
# Which loaded TEV registers the uniform block may give as their low 8 bits (TevLowByteRegs); host only.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$(mktemp -d)}
mkdir -p "$out"
src="$root/platform/vgx/src"
"${CXX:-c++}" -std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -I"$src" -I"$root/platform/vgx/third_party" \
  "$root/tests/vgx_tev_lowbyte.cpp" "$src/vgx_shadergen.cpp" -o "$out/vgx_tev_lowbyte"
"$out/vgx_tev_lowbyte"
