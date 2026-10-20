# Zircon — the Better Android

Zircon is a Google-free, Java-free mobile/desktop OS built on the
[CodeOS kernel](https://github.com/tech-for-everyone/CodeOS-Kernel).

## Why

Android apps are written in Java, running on Dalvik/ART — a virtual machine
that burns RAM and CPU. Zircon replaces that stack with native C apps,
optional Python scripting, and a lightweight compositor. No VM overhead.
No Google services. No sign-in required.

## Features

- **Native C apps** — no VM, no garbage collector, no bloat.
- **Adaptive GUI** — same UI scales from phone to laptop.
- **CodeOS kernel** — fast, minimal, GPLv3.
- **Google-free** — no Play Services, no tracking, no sign-in.
- **Extensible** — write apps in C, Rust, Python.
- **Built-in apps** — Kall, SMS, Settings, Camera, Clock, Calculator, Music.
  Gallery, Browser, Notes, Files, Terminal, Weather, About, Notifications.
- **App Store** — a catalogue of every app with its version, size and
  description, and the installed set behind it: tap a row to open the app,
  tap its badge to take it off (or put it back on) the Home screen. The
  store owns that set, so the Home grid and the Store can never disagree.
- **Real app screens** — ZircApp hosts each built-in app on its own screen:
  a seven-segment Clock that reads the CMOS RTC, a working integer
  Calculator, a Kall dial pad, Notes, an in-app Terminal, Music,
  Settings toggles, Gallery, SMS, Weather, Browser, Camera, About and the
  Store, all over the same fd 3/4 frame protocol.
- **OpenWeb browser** — Rust-powered HTTP + HTML rendering (libow_http.a), no WebKit.
- **bas programs** — `.ba` sources cross-compiled to real Zircon ELFs
  (`/bin/bas-hello`, `/bin/bas-selftest`) and run at boot by a kernel probe,
  so a broken toolchain cannot pass unnoticed. Optional: without a `bas`
  checkout the build is unchanged.
- **Packaged userland** — every part of the OS is a package under `pkgs/`.

## Directory structure

The userland is a tree of **packages** under `pkgs/`; see `pkgs/README.md`
for the convention and how to add one.

```
pkgs/core/base/  — system layer: ABI headers, libc + crt0, ZircApp, the GUI
                   toolkit, the compositor (zircond) and PID 1 (zircon_init)
pkgs/extra/      — OpenWeb bridge + render/network shims for libow_http.a
pkgs/apps/       — one package per built-in app
pkgs/template/   — copy-me package skeleton
kernel/          — Kernel integration & installer
```

## Building

```sh
make          # builds every package, then zircond + zircon_init + zircon.elf
make iso      # builds the bootable Zircon ISO
make -C pkgs  # builds only the packages
make clean
```

The kernel is built in-tree from the vendored sources under `kernel/`, so
`make` here produces a complete, bootable OS.

## Checking it

```sh
python3 scripts/zircon_boot_check.py   # boots the ISO and reads the serial log
```

It asserts that the kernel booted, that the bas probe ran
`/bin/bas-selftest` to completion (`BAS: done status=0`), and that the
compositor and the app store came up. A framebuffer is required, so the
check uses `-vga std -display none`; with `-vga none` the kernel has no
framebuffer and Zircon's userspace launch is skipped.

## Installing

```sh
sudo bash kernel/install.sh
```

## License

GPLv3 License
