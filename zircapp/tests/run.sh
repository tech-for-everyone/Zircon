#!/bin/sh
# Host test for the ZircApp framework.
#
# Copies the real framework sources into a scratch dir so this test's shim
# ABI header shadows Zircon's (which pulls in the freestanding stdint.h) and
# builds a normal host binary.  Run:  sh Zircon/zircapp/tests/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
zircon=$(cd "$here/../.." && pwd)

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

cp "$zircon/zircapp.h"         "$out/"
cp "$zircon/zircapp/zircapp.c" "$out/"
cp "$here/shim/zircon_abi.h"   "$out/"
cp "$here/test_zircapp.c"      "$out/"

cc=${CC:-cc}
"$cc" -std=c99 -Wall -Wextra -Werror -O2 -I"$out" \
    -o "$out/test_zircapp" "$out/zircapp.c" "$out/test_zircapp.c"
"$out/test_zircapp"
