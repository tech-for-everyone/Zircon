#!/usr/bin/env python3
"""Runtime check for the Zircon image: boot the ISO and read the serial log.

The Zircon build can be entirely green and still ship nothing usable: the
kernel can fail to link into a loadable ELF (limine refuses overlapping
program headers), the bas programs can be missing from the initramfs, and the
compositor can fail to start.  None of that shows up at build time, so this
boots the ISO and asserts on what came out of the serial port.

What it checks:

  - the kernel booted (no PANIC, no page fault),
  - the bas probe ran /bin/bas-selftest and it exited 0 -- that is the
    zircon-target cross build, the Zircon libc and the SDK's syscall wrappers
    all working together inside the image,
  - the compositor started.

A framebuffer is required: `-vga std -display none`.  With `-vga none` the
kernel has no framebuffer, Zircon's userspace launch is skipped, and the run
looks like a Zircon failure when it is not one.

Usage:
    zircon_boot_check.py [--iso ISO] [--timeout N] [--negate]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")

# The bas selftest runs on Zircon too (same .ba source, zircon target), so the
# same assertions apply.  These are the values, not the whole output.
EXPECTED = [
    "bas-selftest: start",
    "bas-selftest: vec len = 514",
    "bas-selftest: vec mismatches = 0",
    "bas-selftest: vec sum = 392449",
    "bas-selftest: vec sum correct: true",
    "bas-selftest: str len = 41",
    "bas-selftest: str ends with !: true",
    "bas-selftest: str starts with xxxxx: true",
    "bas-selftest: str contains xx: true",
    "bas-selftest: formatted = 2+3=5",
    "bas-selftest: assert/assert_eq survived",
    "bas-selftest: point dist2 = 25",
    "bas-selftest: rect area = 6",
    "bas-selftest: access /bin/shell ok: true",
    "bas-selftest: open /bin/shell >= 0: true",
    "bas-selftest: missing open is an error: true",
    "bas-selftest: missing stat is an error: true",
    "bas-selftest: missing access is an error: true",
    "bas-selftest: wrote bytes = 24",
    "bas-selftest: read len = 24",
    "bas-selftest: read matches: true",
    "bas-selftest: /bin listing non-empty: true",
    "bas-selftest: survived sleep",
    "bas-selftest: isqrt(10000) = 100",
    "bas-selftest: gcd(48,18) = 6",
    "bas-selftest: done",
]

# Lines whose *value* depends on the machine rather than on the code.
#
# "time > 0" is the uptime in seconds: SYSCALL_TIME returns
# timer_get_milliseconds()/1000, so it is 0 during the first second of boot.
# The Zircon probe runs about half a second in, so the value is genuinely
# "false" here, while the same program on the CodeOS image reads "true"
# because that boot takes several seconds.  The line has to be *there* --
# the program got that far -- but its value is not a property of the
# integration.
FLEXIBLE = [
    r"bas-selftest: time > 0: (true|false)",
]

KERNEL = [
    r"BAS: spawned tid=\d+",
    r"BAS: load /bin/bas-selftest",
    r"BAS: start entry=0x[0-9a-f]+ rsp=0x[0-9a-f]+",
    r"BAS: done status=0",
]

# The compositor, the app store and the init process.  The store line is the
# store's own output: the catalogue is loaded when the Home screen registers
# the apps, so its absence means the store package is not in the image.
COMPOSITOR = [
    r"zircon: compositor launched \(PID \d+\)",
    r"Zircon ready",
    r"store: catalogue \d+ apps",
    r"home: all apps registered \(\d+ apps\)",
]

# A failure anywhere is a failure, even if every expected line is present.
REJECT = [
    r"BAS: FAIL",
    r"BAS: selftest timed out",
    r"!!! PF at RIP",
    r"PANIC:",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default="build/zircon.iso")
    ap.add_argument("--timeout", type=int, default=240)
    ap.add_argument("--negate", action="store_true")
    args = ap.parse_args()

    logfile = os.path.join(tempfile.mkdtemp(prefix="zirconboot"), "serial.log")
    qemu = [
        "qemu-system-x86_64", "-machine", "pc", "-m", "1G", "-smp", "2",
        "-vga", "std", "-display", "none", "-monitor", "none",
        "-boot", "order=d", "-cdrom", args.iso, "-no-reboot",
        "-serial", f"file:{logfile}",
    ]
    proc = subprocess.Popen(qemu, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        end = time.time() + args.timeout
        while time.time() < end:
            time.sleep(2)
            if os.path.exists(logfile):
                with open(logfile, "r", errors="replace") as f:
                    log = ANSI.sub("", f.read())
                if "BAS: done" in log and "zircapp:" in log:
                    break
            if proc.poll() is not None:
                break
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()

    log = ""
    if os.path.exists(logfile):
        with open(logfile, "r", errors="replace") as f:
            log = ANSI.sub("", f.read())

    failures = []
    for pat in KERNEL:
        if not re.search(pat, log):
            failures.append(f"kernel log missing: {pat}")
    for line in EXPECTED:
        if line not in log:
            failures.append(f"missing: {line}")
    for pat in COMPOSITOR:
        if not re.search(pat, log):
            failures.append(f"compositor log missing: {pat}")
    for pat in FLEXIBLE:
        if not re.search(pat, log):
            failures.append(f"missing: {pat}")
    for pat in REJECT:
        if re.search(pat, log):
            failures.append(f"rejected pattern present: {pat}")

    print("=== zircon serial log ===")
    for line in log.splitlines():
        if ("BAS" in line or "bas" in line or "zircapp" in line or "zircon:" in line
                or "store" in line or "home" in line or "Zircon" in line):
            print("   ", line.strip())
    print()

    if failures and args.negate:
        print(f"NEGATIVE CONTROL: {len(failures)} checks failed, as expected")
        return 0
    if failures:
        print("FAIL")
        for f in failures:
            print("  -", f)
        return 1
    print(f"PASS: {len(EXPECTED)} bas program assertions, {len(KERNEL)} "
          f"kernel-probe lines and the compositor banner verified on a real "
          f"Zircon boot")
    return 0


if __name__ == "__main__":
    sys.exit(main())
