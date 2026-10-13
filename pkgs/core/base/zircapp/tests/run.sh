#!/bin/sh
# Host test for the ZircApp framework.
#
# Copies the real framework sources into a scratch dir so this test's shim
# ABI header shadows Zircon's (which pulls in the kernel types) and builds a
# normal host binary.
#   sh Zircon/pkgs/core/base/zircapp/tests/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
pkg=$(cd "$here/.." && pwd)          # the zircapp package directory

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

cp "$pkg/include/zircapp.h" "$out/"
cp "$pkg/src/zircapp.c"     "$out/"
cp "$here/shim/zircon_abi.h" "$out/"
cp "$here/test_zircapp.c"    "$out/"

cc=${CC:-cc}
"$cc" -std=c99 -Wall -Wextra -Werror -O2 -I"$out" \
    -o "$out/test_zircapp" "$out/zircapp.c" "$out/test_zircapp.c"
"$out/test_zircapp"
