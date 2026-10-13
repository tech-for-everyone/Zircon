# Zircon packages

Every piece of the Zircon userland — the system layer, the framework, the
GUI toolkit, the browser bridge and the apps — is a **package**: a directory
under `pkgs/` with a `manifest.json`, a `Makefile` and its sources.  The
top-level `Zircon/Makefile` builds them all and links the results into the
`zircond` compositor and the `zircon_init` PID 1.

```
pkgs/
  pkgs.mk        shared toolchain + include paths (one source of truth)
  package.mk     shared compile/archive rules included by every package
  Makefile       discovers every manifest.json and builds those packages
  template/      copy-me skeleton (deliberately not built)
  core/base/     the system layer
    abi/         kernel ABI headers (syscalls, IPC, FB/input)
    libc/        freestanding libc + crt0 + the freestanding headers
    zircapp/     the ZircApp application framework
    gui/         the GUI toolkit (panels, widgets, windows)
    init/        PID 1
    compositor/  zircond: the app tree, input pump and frame loop
  extra/browser/ OpenWeb bridge + HTML/network shims (Rust libow_http.a)
  apps/<name>/   one package per built-in app
```

## The package layout

```
pkgs/<tier>/<name>/
  manifest.json      metadata: name, version, description, category,
                     dependencies, install_path, source
  Makefile           per-package build (4 lines; see below)
  src/               sources (and any header private to the package)
  include/           public headers, when the package exports any
```

## Adding a package

1. Copy the template:

   ```sh
   cp -r pkgs/template pkgs/apps/hello
   ```

2. Edit `manifest.json` (at least `name`, `description`, `install_path`) and
   rename the source file inside `src/`.

3. Register it if it is an app — add its header and `zircapp_register(&x_app)`
   to `pkgs/apps/home/src/home.c`.

4. Build:

   ```sh
   make -C pkgs core/base/gui   # just this package
   make -C pkgs                 # every package
   make                         # packages + images, from Zircon/
   ```

That is all: `pkgs/Makefile` finds the new `manifest.json` on its own, and the
top-level link step picks up the new objects automatically.

## The per-package Makefile

Every package Makefile is the same handful of lines; all the toolchain and
include-path knowledge lives in `pkgs/pkgs.mk` / `pkgs/package.mk`:

```make
PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")/pkgs/package.mk
```

`package.mk` compiles each source into `build/pkgs/<package-path>/` and
archives it as `lib<name>.a`.  Nothing is written inside the package tree, so
a checkout stays clean.

## Notes

- `desktop.h`, pulled in by `gui/*.h`, resolves to the **kernel** header
  (`kernel/kernel/desktop.h`), which is why `pkgs.mk` puts the kernel headers
  on the include path.  The `zircapp/port/` shims exist only for the host
  tests, which build the framework against a normal libc.
- `<string.h>` / `<stdio.h>` resolve to the freestanding copies in
  `pkgs/core/openssl/freestd`, which must come **before** `kernel/kernel`
  on the include path (the kernel's `string.h` has `strncpy_safe`, not
  `strncpy`).
- Only the x86_64 userland is package-based.  The optional ARM64 target
  builds the SM-G960U kernel bring-up straight from `kernel/arch/arm64`.
