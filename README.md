# Zircon — the Better Android

Zircon is a Google-free, Java-free mobile/desktop OS built on the
[CodeOS](https://github.com/tech-for-everyone/CodeOS) kernel.

## Why

Android apps are written in Java, running on Dalvik/ART — a virtual machine
that burns RAM and CPU. Zircon replaces that stack with native C apps,
optional Python scripting, and a lightweight compositor. No VM overhead.
No Google services. No sign-in required.

## Features

- **Native C apps** — no VM, no garbage collector, no bloat
- **Adaptive GUI** — same UI scales from phone to laptop
- **CodeOS kernel** — fast, minimal, GPLv3
- **Google-free** — no Play Services, no tracking, no sign-in
- **Extensible** — write apps in C or Python

## Directory structure

```
.gui/          — GUI toolkit (panels, widgets, windows)
.apps/         — Built-in apps
.kernel/       — Kernel integration & installer
.github/       — CI
```

## Building

```sh
make          # builds libzircon.a + zircond
make lib      # builds libzircon.a only
make clean
```

## Installing

```sh
sudo bash .kernel/install.sh
```

## License

MIT — Copyright (c) 2026 gitdev
