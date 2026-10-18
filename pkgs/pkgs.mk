# ╔══════════════════════════════════════════════════════════════╗
# ║  Zircon — package build configuration (shared)                 ║
# ╚══════════════════════════════════════════════════════════════╝
#
# Every Zircon package lives in its own directory with a manifest.json,
# a Makefile and its sources, and builds itself through pkgs/package.mk.
# This file holds the one thing they all share: the toolchain and the
# include paths.  Nothing here writes into a package directory -- all
# output lands under $(ZROOT)/build/pkgs/<package-path>/.

# Locate the Zircon root by walking up until pkgs/pkgs.mk is found.  This
# makes a package Makefile work whether it is invoked by the top-level
# Makefile, by pkgs/Makefile, or straight from its own directory.
ZROOT := $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")

BUILD      ?= $(ZROOT)/build
KERNEL_DIR ?= $(ZROOT)/../kernel

# Package identity, derived from where this file is being included from.
PKG_DIR   := $(patsubst $(ZROOT)/pkgs/%,%,$(CURDIR))
PKG_NAME  := $(notdir $(CURDIR))
PKG_BUILD := $(BUILD)/pkgs/$(PKG_DIR)
PKG_LIB   := $(PKG_BUILD)/lib$(PKG_NAME).a

# ── Toolchain ──
# clang targets the freestanding CodeOS userland.  `ar`/`objcopy` stay GNU
# binutils unless the ELF cross tools are present; they only archive objects
# and never touch codegen.
CC  := clang --target=x86_64-unknown-none
AS  := clang --target=x86_64-unknown-none
AR  := $(shell command -v x86_64-elf-ar >/dev/null 2>&1 && echo x86_64-elf-ar || echo ar)
ARFLAGS := rcs
MKDIR := mkdir -p

PKG_CFLAGS ?= -std=c99 -Wall -Wextra -O2 -g \
	-ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone

# ── Include paths ──
#  * the public headers of the base packages (libc, ABI, ZircApp, gui);
#  * the kernel headers, because gui/*.h pull `desktop.h` -> kernel types;
#  * the freestanding stdio/OpenWeb headers shared with the CodeOS userspace;
#  * every app package's own src/, because home.c includes its siblings.
APP_INCLUDES := $(addprefix -I$(ZROOT)/pkgs/apps/,\
	$(addsuffix /src,$(notdir $(wildcard $(ZROOT)/pkgs/apps/*))))

# Order matters: `desktop.h` (pulled in by gui/*.h) has to reach the kernel
# header, while `<string.h>`/`<stdio.h>` must resolve to the freestanding
# copies in pkgs/core/openssl/freestd *before* the kernel's own string.h
# (which has strncpy_safe, not strncpy).
CPPFLAGS := \
	-I$(ZROOT)/pkgs/core/base/libc/include \
	-I$(ZROOT)/pkgs/core/base/abi/include \
	-I$(ZROOT)/pkgs/core/base/zircapp/include \
	-I$(ZROOT)/pkgs/core/base/gui/include \
	$(APP_INCLUDES) \
	-I$(ZROOT)/pkgs/extra/browser/src \
	-I$(ZROOT)/../qt6/panels \
	-I$(ZROOT)/../pkgs/core/panels/src \
	-I$(ZROOT)/../pkgs/core/openssl/freestd \
	-I$(KERNEL_DIR)/kernel \
	-I$(ZROOT)/../qt6/posixstubs/include \
	-I$(ZROOT)/../qt6/posixstubs/include/sys \
	-I$(ZROOT)/../qt6/sysroot/usr/include \
	-I$(ZROOT)/kernel/userspace/include \
	-D_GNU_SOURCE

# Link flags for the images assembled by the top-level Makefile.  `none` is a
# freestanding target, so clang does not reach for the host runtime; these
# keep the resulting ELF static and 4K-page aligned for the kernel loader.
LDFLAGS ?= -nostdlib -static -z max-page-size=0x1000 -z noexecstack
LDLIBS  ?=
