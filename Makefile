# ╔══════════════════════════════════════════════════════════════╗
# ║  Zircon OS — top-level build                                   ║
# ║  Google-free mobile/desktop OS built on the CodeOS kernel       ║
# ╚══════════════════════════════════════════════════════════════╝
#
# The userland is a tree of packages under pkgs/ (each with its own
# manifest.json + Makefile).  This Makefile builds them and links the
# results into the Zircon images, then drives the ISO / QEMU / ARM64
# targets.  See pkgs/README.md for the package convention.

ARCH ?= x86_64
ZROOT := $(CURDIR)
# Zircon now vendors its own copy of the CodeOS kernel under kernel/ and builds
# it here, instead of consuming the parent CodeOS tree's build output.
KERNEL_DIR := kernel

# ── Architecture selection ──
ifeq ($(ARCH),arm64)
    # The ARM64 target builds the Galaxy S9 (SM-G960U) kernel bring-up only;
    # the pkgs/ userland is x86_64-only.
    CC := clang --target=aarch64-none-elf
    AR := $(shell command -v aarch64-linux-gnu-ar >/dev/null 2>&1 && echo aarch64-linux-gnu-ar || echo ar)
    ARFLAGS := rcs
    OBJCOPY := $(shell command -v aarch64-linux-gnu-objcopy >/dev/null 2>&1 && echo aarch64-linux-gnu-objcopy || echo objcopy)
    LD := $(shell command -v ld.lld >/dev/null 2>&1 && echo "ld.lld -m aarch64elf" || echo "ld -m aarch64elf")
    BUILD := build/arm64
    ISO_DIR := build/iso-arm64
    ISO_ROOT := $(ISO_DIR)/iso_root
    MKDIR := mkdir -p
    CP := cp
    CFLAGS := -std=c99 -Wall -Wextra -pedantic -g -ffreestanding -nostdlib -nostartfiles -fno-stack-protector -fno-PIC -mgeneral-regs-only
    CPPFLAGS := -I. -Ipkgs -I$(KERNEL_DIR)/arch/arm64 -Ikernel/arch/arm64/dts -D__ARM64__ -D__ZIRCON_ARM64__
    LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack -T kernel/arch/arm64/linker.ld
else
    # ── x86_64 userland ──
    # Toolchain, include paths and build layout are shared with every package
    # through pkgs/pkgs.mk; this branch owns only the image-level rules.
    include pkgs/pkgs.mk
    BUILD := $(ZROOT)/build
    ISO_DIR := $(BUILD)/iso
    ISO_ROOT := $(ISO_DIR)/iso_root
    MKDIR := mkdir -p
    CP := cp
    # The objects are built -fno-pic/-fno-pie, so the link must not default to
    # a PIE (host ld otherwise dies with "R_X86_64_32S ... can not be used when
    # making a PIE object").
    LDFLAGS := -nostdlib -static -no-pie -z max-page-size=0x1000 -z noexecstack
    LDLIBS :=

    # Propagate the (possibly target-specific) package CFLAGS into the
    # sub-make so `make debug` / `make release` actually rebuild the packages.
    export PKG_CFLAGS
endif

# ── Images ──

ZIRCON_KERNEL := $(KERNEL_DIR)/codeos-1-kernel.bin
ZIRCON_KERNEL_ARM64 := $(KERNEL_DIR)/zircon-kernel-arm64.bin
ZIRCON_DTB := $(BUILD)/zircon-sm-g960u.dtb
ZIRCON_INIT := $(BUILD)/zircon_init
ZIRCON_COMPOSITOR := $(BUILD)/zircond
ZIRCON_ELF := $(BUILD)/zircon.elf
ZIRCON_ISO := $(BUILD)/zircon.iso
ZIRCON_ISO_ARM64 := $(BUILD)/zircon-arm64.iso
RUST_LIB := $(KERNEL_DIR)/kernel/rust_ow/target/x86_64-unknown-none/release/libow_http.a

# Limine stage-3 image.  Do NOT use $(KERNEL_DIR)/bootloader/limine-bios.sys:
# that is a stale 351944-byte copy which does not match limine-bios-cd.bin,
# and limine rejects it at boot with "limine-bios.sys size incorrect" /
# "PANIC: Failed to load stage 3".  The matching 302836-byte file is the one
# CodeOS's own working ISO ships, from its iso_root.
LIMINE_BIOS_SYS := $(KERNEL_DIR)/bootloader/iso_root/boot/limine-bios.sys
LIMINE_DEPLOY := $(KERNEL_DIR)/bootloader/limine-deploy

# ── Package build ──
# `pkgs` is phony, so the link rules always relink against the current set of
# package objects.  ZIRCOND_OBJS / INIT_OBJS are recursive variables: they are
# expanded inside the recipe, i.e. after the sub-make has produced the objects.

.PHONY: all clean debug release run iso iso-arm64 pkgs zircapp-tests

ZIRCOND_OBJS = $(shell find $(BUILD)/pkgs -name '*.o' ! -path '*/init/*' 2>/dev/null)
INIT_OBJS    = $(shell find $(BUILD)/pkgs/core/base/libc $(BUILD)/pkgs/core/base/init -name '*.o' 2>/dev/null)

ifeq ($(ARCH),x86_64)

# bas programs: .ba sources cross-compiled into Zircon userland ELFs by the
# optional sibling bas checkout.  Included (not inlined) so Zircon/Makefile
# stays the owner of the image link and pkgs/bas.mk owns the toolchain.
include pkgs/bas.mk

all: pkgs $(ZIRCON_COMPOSITOR) $(ZIRCON_INIT) $(ZIRCON_ELF) bas_progs

debug: PKG_CFLAGS += -O0 -DDEBUG
debug: all
release: PKG_CFLAGS += -O2 -DNDEBUG
release: all

# Build every package (each owns its Makefile).
.PHONY: pkgs
pkgs:
	@$(MAKE) -s -C pkgs all

# ── Rust library (OpenWeb) ──
# Built as part of the vendored kernel tree; delegate so the kernel Makefile's
# pinned Rust toolchain path is used.
$(RUST_LIB):
	$(MAKE) -C $(KERNEL_DIR) kernel/rust_ow/target/x86_64-unknown-none/release/libow_http.a

# ── Link the compositor (zircond) ──
# Every package object except init's, plus the Rust OpenWeb archive.  The
# `<pkg>/lib<pkg>.a` archives are built for packaging; linking the objects
# directly keeps crt0.o's _start unconditionally present.
$(ZIRCON_COMPOSITOR): pkgs $(RUST_LIB)
	$(MKDIR) $(@D)
	$(CC) -nostartfiles -o $@ $(ZIRCOND_OBJS) $(RUST_LIB) $(LDFLAGS) $(LDLIBS)

# ── Link PID 1 (zircon_init) ──
$(ZIRCON_INIT): pkgs
	$(MKDIR) $(@D)
	$(CC) -nostartfiles -o $@ $(INIT_OBJS) $(LDFLAGS) $(LDLIBS)

# ── Zircon ELF (kernel + initramfs) ──
# The kernel is now built in-tree from the vendored CodeOS kernel sources in
# kernel/.  `kernel` is phony so the sub-make always runs; it is incremental
# after the first (cold) build.
.PHONY: kernel
# bas_progs first, not merely alongside: the kernel's initramfs rule embeds
# ../build/bin/bas-* and its wildcard is expanded at sub-make time, so the
# files must already exist.  A clean `make` would otherwise produce an image
# without them and only a second run would include them.
kernel: bas_progs
	$(MAKE) -C $(KERNEL_DIR) all

$(ZIRCON_KERNEL): kernel

# zircon.elf is a copy of that kernel image (kernel + embedded initramfs).
$(ZIRCON_ELF): $(ZIRCON_KERNEL)
	$(MKDIR) $(@D)
	$(CP) $< $@
endif

ifeq ($(ARCH),arm64)

ZIRCON_ELF :=

all: $(ZIRCON_KERNEL_ARM64)

ARM64_C_SRC := $(wildcard kernel/arch/arm64/*.c)
ARM64_S_SRC := $(wildcard kernel/arch/arm64/*.S)
ARM64_OBJ := $(patsubst kernel/arch/arm64/%.c,$(BUILD)/%.o,$(ARM64_C_SRC)) \
             $(patsubst kernel/arch/arm64/%.S,$(BUILD)/%.o,$(ARM64_S_SRC))

$(ZIRCON_KERNEL_ARM64): $(ARM64_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: kernel/arch/arm64/%.c
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: kernel/arch/arm64/%.S
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) -c $< -o $@

# DTB compilation
$(ZIRCON_DTB): kernel/arch/arm64/dts/sm-g960u.dts
	$(MKDIR) $(dir $@)
	dtc -I dts -O dtb -o $@ $<

endif # ARCH == arm64

# ── Initramfs + ISO (x86_64) ──

initramfs: $(ZIRCON_INIT) $(ZIRCON_COMPOSITOR) $(ZIRCON_KERNEL) bas_progs
	$(MKDIR) $(ISO_ROOT)/boot $(ISO_ROOT)/sbin $(ISO_ROOT)/bin
	$(CP) $(ZIRCON_INIT) $(ISO_ROOT)/sbin/zircon_init
	$(CP) $(ZIRCON_COMPOSITOR) $(ISO_ROOT)/sbin/zircond
	$(CP) $(ZIRCON_KERNEL) $(ISO_ROOT)/boot/zircon-kernel.bin
	# bas programs built from pkgs/core/bas/src/*.ba (see pkgs/bas.mk).  The
	# loop tolerates an absent bas checkout by matching nothing, so this is a
	# no-op rather than a failure when bas is not present.
	for p in $(BAS_BINS); do \
	    if [ -f "$$p" ]; then $(CP) "$$p" $(ISO_ROOT)/bin/; fi; \
	done; chmod 755 $(ISO_ROOT)/bin 2>/dev/null || true
	echo '#!/bin/sh' > $(ISO_ROOT)/init
	echo 'echo "Zircon OS v1.0.0"' >> $(ISO_ROOT)/init
	echo '/sbin/zircon_init' >> $(ISO_ROOT)/init
	chmod +x $(ISO_ROOT)/init

iso: initramfs
	$(MKDIR) $(ISO_ROOT)/boot/limine
	$(CP) $(KERNEL_DIR)/bootloader/limine-bios-cd.bin $(ISO_ROOT)/boot/
	$(CP) $(KERNEL_DIR)/bootloader/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	$(CP) $(LIMINE_BIOS_SYS) $(ISO_ROOT)/boot/
	$(CP) bootloader/limine.conf $(ISO_ROOT)/boot/limine.conf
	$(CP) $(KERNEL_DIR)/bootloader/splash.png $(ISO_ROOT)/boot/
	# Zircon's limine.conf sets `wallpaper: boot():/boot/splash.png`.
	# These xorriso flags mirror the kernel's working codeos-1.0.iso rule.
	xorriso -as mkisofs \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(ZIRCON_ISO)
	# limine-deploy patches the stage-2 copy inside the image with the config
	# location. Without it the BIOS finds the El Torito image, prints
	# "Booting from DVD/CD..." and then hangs forever with no diagnostic.
	$(LIMINE_DEPLOY) bios-install $(ZIRCON_ISO)
	@echo "Zircon ISO built: $(ZIRCON_ISO)"

# ── Boot check ──
#
# Boots the ISO and asserts on the serial log: the kernel came up, the bas
# probe ran /bin/bas-selftest to completion, and the compositor and the app
# store are alive.  A green build proves none of that.
.PHONY: boot-check
boot-check: iso
	@python3 scripts/zircon_boot_check.py --iso $(ZIRCON_ISO)

# ── Store check ──
#
# The store's screen needs a framebuffer, so its *logic* -- the catalogue and
# the installed set that the Home grid reads -- is tested on the host.
.PHONY: store-check
store-check:
	@cc -std=c11 -Wall -Wextra \
	    -I$(ZROOT)/pkgs/apps/store/src -I$(ZROOT)/pkgs/core/base/zircapp/include \
	    -o /tmp/zircon-store-test \
	    $(ZROOT)/pkgs/apps/store/tests/store_test.c $(ZROOT)/pkgs/apps/store/src/store.c
	@/tmp/zircon-store-test

# ── Run ──
#
# The kernel cmdline lives in the limine config, not on the QEMU command line:
# -append is rejected without -kernel, and this boots an ISO through limine.
run: iso
	qemu-system-x86_64 \
		-cpu Haswell,+smep \
		-m 4096 \
		-smp 4 \
		-machine q35 \
		-cdrom $(ZIRCON_ISO) \
		-boot order=d \
		-serial stdio \
		-display sdl \
		-vga std
	# NOT +smap: the CodeOS kernel faults immediately once it turns SMAP on
	# (PF at CR2=0xffff800000241208 err=1).  SMEP alone is fine, so this is
	# worked around here rather than fixed; re-enable +smap once the kernel
	# side is sorted.

# ── ISO (ARM64 for Samsung Galaxy S9) ──

iso-arm64: $(ZIRCON_KERNEL_ARM64) $(ZIRCON_DTB)
	$(MKDIR) $(ISO_ROOT)/boot
	$(CP) $(ZIRCON_KERNEL_ARM64) $(ISO_ROOT)/boot/zircon-kernel-arm64.bin
	$(CP) $(ZIRCON_DTB) $(ISO_ROOT)/boot/zircon-sm-g960u.dtb
	$(CP) bootloader/limine.conf $(ISO_ROOT)/boot/limine.conf
	$(CP) $(KERNEL_DIR)/bootloader/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	$(CP) $(KERNEL_DIR)/bootloader/splash.png $(ISO_ROOT)/boot/
	echo '#!/bin/sh' > $(ISO_ROOT)/init
	echo 'echo "Zircon OS ARM64 v1.0.0"' >> $(ISO_ROOT)/init
	echo 'echo "Booting on Samsung Galaxy S9 (SM-G960U)"' >> $(ISO_ROOT)/init
	chmod +x $(ISO_ROOT)/init
	xorriso -as mkisofs \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(ZIRCON_ISO_ARM64)
	@echo "Zircon ARM64 ISO built: $(ZIRCON_ISO_ARM64)"

# ── Host tests (no kernel, no boot) ──
# Phase 1 drives the framework itself (tests/run.sh); Phase 2 drives the
# windowed host's WM encoder and event pump (tests/host/run.sh).  Both
# build the real sources against shims.  Run the second with ZC_CONTROL=1
# to prove the test is sensitive to the record-boundary logic.
zircapp-tests:
	sh pkgs/core/base/zircapp/tests/run.sh
	sh pkgs/core/base/zircapp/tests/host/run.sh

.PHONY: initramfs zircapp-tests

# ── Clean ──

clean:
	$(MAKE) -s -C pkgs clean
	rm -rf $(BUILD)
