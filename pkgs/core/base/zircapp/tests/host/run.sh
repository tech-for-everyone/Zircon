#!/bin/sh
# Phase 2 host test: drives the WM encoder / event pump / Home grid logic
# that a headless boot cannot reach.
#
# Builds the *real* Zircon framework (pkgs/core/base/zircapp/src/zircapp.c)
# and the *real* CodeOS userspace ZircApp (pkgs/core/zircapp/src/zircapp.c)
# against in-memory shims for the kernel fd 3/4 bridge.
#
#   sh Zircon/pkgs/core/base/zircapp/tests/host/run.sh
#   ZC_CONTROL=1 sh Zircon/pkgs/core/base/zircapp/tests/host/run.sh  # negative control
set -e
here=$(cd "$(dirname "$0")" && pwd)          # .../zircapp/tests/host
tests=$(cd "$here/.." && pwd)                # .../zircapp/tests
pkg=$(cd "$here/../.." && pwd)               # .../zircapp
root=$(cd "$pkg/../../../../.." && pwd)      # CodeOS repo root

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

cp "$pkg/include/zircapp.h"                       "$out/"
cp "$pkg/src/zircapp.c"                           "$out/framework.c"
cp "$root/pkgs/core/zircapp/src/zircapp.c"        "$out/host.c"
cp "$root/kernel/userspace/include/wm_protocol.h" "$out/"
cp "$tests/shim/zircon_abi.h"                     "$out/"
cp "$here/shim/unistd.h"                          "$out/unistd.h"
cp "$here/test_host.c"                            "$out/"

if [ "${ZC_CONTROL:-0}" = "1" ]; then
    # Negative control: mis-declare the KEY record length.  The pump can
    # then no longer decode the Esc key, so the back/close assertions must
    # fail.  A control that still passes would mean the test is not
    # actually measuring the record-boundary logic.
    sed -i 's/\(WM_EVENT_KEY:\)[[:space:]]*len = 6/\1    len = 8/' "$out/host.c"
    if ! grep -q 'WM_EVENT_KEY:    len = 8' "$out/host.c"; then
        echo "control: patch did not apply" >&2
        exit 2
    fi
fi

cc=${CC:-cc}
"$cc" -std=c99 -Wall -Wextra -Werror -O2 -I"$out" \
    -o "$out/test_host" "$out/framework.c" "$out/test_host.c"

set +e
"$out/test_host"
rc=$?
set -e

if [ "${ZC_CONTROL:-0}" = "1" ]; then
    if [ "$rc" -eq 0 ]; then
        echo "control FAILED to fail: mis-declared KEY length still passed"
        exit 1
    fi
    echo "control ok: mis-declared KEY length makes the test fail (rc=$rc)"
    exit 0
fi
exit "$rc"
