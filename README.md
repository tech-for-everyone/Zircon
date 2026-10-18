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
- **Built-in apps** — Phone, SMS, Settings, Camera, Clock, Calculator, Music.
  Gallery, Browser, Notes, Files, Terminal, Weather, About, Notifications.
- **Real app screens** — ZircApp hosts each built-in app on its own screen:
  a seven-segment Clock that reads the CMOS RTC, a working integer
  Calculator, a Phone dial pad, Notes, an in-app Terminal, Music,
  Settings toggles, Gallery, SMS, Weather, Browser, Camera and About
  (kernel version + clock), all over the same fd 3/4 frame protocol.
- **OpenWeb browser** — Rust-powered HTTP + HTML rendering (libow_http.a), no WebKit.
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

## Installing

```sh
sudo bash kernel/install.sh
```

## License

GPLv3 License
