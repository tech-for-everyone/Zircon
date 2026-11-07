#!/usr/bin/env python3
"""Boot Zircon's AArch64 Image on QEMU virt and verify serial milestones."""

from __future__ import annotations

import argparse
import subprocess
import sys


EXPECTED = (
    "Zircon AArch64 kernel",
    "early: PL011 console at 0x0000000009000000",
    "early: valid device tree",
    "ZIRCON_ARM64_BOOT_OK",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default="qemu-system-aarch64")
    parser.add_argument("--image", required=True)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    command = [
        args.qemu,
        "-machine", "virt,gic-version=3",
        "-cpu", "cortex-a53",
        "-m", "1G",
        "-nographic",
        "-monitor", "none",
        "-serial", "stdio",
        "-no-reboot",
        "-kernel", args.image,
    ]
    try:
        process = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True,
        )
    except OSError as exc:
        print(f"ARM64 boot check: cannot start QEMU: {exc}", file=sys.stderr)
        return 1

    try:
        output, _ = process.communicate(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()

    print(output, end="" if output.endswith("\n") else "\n")
    missing = [line for line in EXPECTED if line not in output]
    if missing:
        print("ARM64 boot check failed; missing serial milestone(s): " +
              ", ".join(missing), file=sys.stderr)
        return 1
    print("PASS: AArch64 Image booted on QEMU virt, validated its FDT, and reached the kernel entry point")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
