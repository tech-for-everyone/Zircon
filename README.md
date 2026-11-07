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
- **Zircon identity** — the project dragon mark is used on the boot splash
  and rendered by the kernel on every x86_64 startup before Zircon userspace
  begins. It is also shipped as `/usr/share/icons/zircon_logo.png`; its SVG
  source is kept under `kernel/userspace/icons/`. The current ARM64 bring-up
  is serial-only and has no framebuffer boot logo yet.
- **Google-free** — no Play Services, no tracking, no sign-in.
- **Extensible** — write apps in C, Rust, Python.
- **Built-in apps** — Kall, SMS, Settings, Camera, Clock, Calculator, Music.
  Gallery, Browser, Notes, Files, Terminal, Weather, About, Notifications,
  and Move On for preparing personal files for transfer to CodeOS.
- **App Store** — a catalogue of every app with its version, size and
  description, searchable by name/description and filterable by category.
  App details show requested capabilities, and Home actions stay synchronized
  with the Store's installed set. This currently manages built-in apps; it
  does not yet load third-party binaries at runtime.
- **Device support** — the kernel knows the phones and tablets it drives
  (`kernel/kernel/device.h`): a panel, the logical screen the interface is
  laid out for, and the scale between them, so a 1080x2400 phone is a
  360x800 interface at 3x. The compositor asks the kernel which device it
  is running as and lays out for it — two columns of icons on a phone, four
  to six on a tablet — and the bridge scales the surface to the panel. Boot
  the "Zircon (Tablet)" or "Zircon (Phone)" entry to try a layout without
  the hardware.
- **Zircon shell and ZircApp modules** — the compositor owns the home grid,
  navigation and window protocol; it is not itself a Home app. App packages
  register as ZircApp applications and receive foreground lifecycle events.
  Their render callbacks draw through a compositor-provided canvas (the
  Clock, About, Weather, Camera and Gallery use this path); screens not yet
  migrated continue to use the shell's compatibility renderer. Notes has an on-screen
  keyboard and auto-save, Files has live directory browsing and text
  previews, and Settings controls Wi-Fi/Airplane/DND and the light/dark
  theme. Bluetooth is reported as detected/unavailable; radio control is
  not implemented yet.
- **OpenWeb browser** — Rust-powered HTTP + HTML rendering (libow_http.a), no
  WebKit. HTTP and HTTPS GETs go through a bounded kernel syscall; x86_64 TLS
  uses OpenSSL with SNI, certificate-chain and hostname verification. The
  kernel RTC supplies certificate time, and TLS fails closed if it is invalid.
  The current ARM64 bring-up does not yet include networking.
  Add `httpsboot` to the x86_64 kernel command line to run the TLS verification
  smoke test during boot; it checks rejection of an untrusted certificate,
  the development-only insecure override, and a public HTTPS endpoint.
- **bas programs** — `.ba` sources cross-compiled to real Zircon ELFs
  (`/bin/bas-hello`, `/bin/bas-selftest`) and run at boot by a kernel probe,
  so a broken toolchain cannot pass unnoticed. Optional: without a `bas`
  checkout the build is unchanged.
- **Packaged userland** — every part of the OS is a package under `pkgs/`.

## AArch64 bring-up

The AArch64 target now builds a flat, Linux `Image`-compatible kernel and the
Galaxy S9 (SM-G960U) device tree. Its currently verified runnable platform is
QEMU `virt`:

```sh
make ARCH=arm64                         # build Image + SM-G960U DTB
make ARCH=arm64 boot-check-arm64        # boot Image on QEMU and assert serial milestones
make ARCH=arm64 run-arm64               # interactive serial boot
```

The checked boot reaches an early PL011 console, validates QEMU's supplied
flattened device tree, and prints `ZIRCON_ARM64_BOOT_OK`. It is an early kernel
bring-up only: there is no scheduler, memory manager, interrupt controller,
display stack, or Zircon app userland on AArch64 yet. The SM-G960U DTB is built
but its Qualcomm GENI console, Samsung boot chain, and phone peripherals are
not yet brought up; the QEMU boot check does not claim that a physical S9 can
boot this image. `iso-arm64` is retained as a compatibility alias that builds
the raw Image and DTB, not an ISO.

## Directory structure

The userland is a tree of **packages** under `pkgs/`; see `pkgs/README.md`
for the convention and how to add one.

The device-profile API identifies the Galaxy S9 as `sm-g960u` (1440x2960
panel, 360x740 logical UI at 4x scale). On compatible CodeOS boots, select
that profile with `zircon.device=sm-g960u`; this configures layout only and
does not imply working S9 display, touch, or Qualcomm drivers.

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

### Moving data to CodeOS

Launch **Move On** and tap **Prepare export** to copy the user home to
`/root/root/home/Move On/export-NNN`. Copy that directory to a host computer,
then use `Zircon/scripts/move_on.py` to create and import a verified
`.moveon` bundle. The phone exporter currently supports at most 500 entries
and 64 KiB per file, matching the current filesystem syscall limits.

### ZircApp packages and curated repositories

ZircApp modules use the versioned C ABI in
`pkgs/core/base/zircapp/include/zircapp_module_v1.h`; the template includes C
and Rust examples under `pkgs/template/`. Third-party application bundles use
the `zircon-app-1` manifest format and `.xora` payload envelope. A host-side
repository tool validates packages, creates an index, signs it with an
operator-managed Ed25519 key, verifies signatures, and can sync one package
from an HTTPS repository:

```sh
python3 scripts/zircapp_repo.py validate pkgs/template/app-manifest.example.json
python3 scripts/zircapp_repo.py pack /path/to/built-app -o /path/to/repository/example-app-1.0.0.xora
python3 scripts/zircapp_repo.py index /path/to/repository -o index.json
python3 scripts/zircapp_repo.py sign index.json --key /secure/repo-private.pem \
  --signature index.sig
python3 scripts/zircapp_repo.py verify index.json --signature index.sig \
  --public-key repo-public.pem
python3 scripts/zircapp_repo.py sync https://apps.example.org/ \
  --public-key repo-public.pem --name example-app --root /path/to/zircon-root
```

Never put the repository signing private key in the source tree. The tool
installs verified payloads beneath `/apps/<name>` in the selected filesystem
root. Zircon's Store does not yet download or execute these packages; that
requires the separate hosted-app runtime and an OS-shipped curated trust key.
Capability names are currently disclosure metadata, not enforced runtime
permissions.

The kernel is built in-tree from the vendored sources under `kernel/`, so
`make` here produces a complete, bootable OS.

## Checking it

```sh
make store-check                    # the store's catalogue and installed set (host)
make zircapp-repo-check             # signed repository and safe install checks
python3 scripts/zircon_boot_check.py   # boots the ISO and reads the serial log
```

`store-check` compiles the store's logic against the real source and drives
it: the catalogue, and the installed set the Home grid reads. The boot check
asserts that the kernel booted, that the bas probe ran `/bin/bas-selftest` to
completion (`BAS: done status=0`), and that the compositor and the app store
came up. A framebuffer is required, so the check uses `-vga std -display
none`; with `-vga none` the kernel has no framebuffer and Zircon's userspace
launch is skipped.

## Installing

```sh
sudo bash kernel/install.sh
```

## License

GPLv3 License
