# ╔══════════════════════════════════════════════════════════════╗
# ║  Zircon OS — Build System                                      ║
# ║  Google-free mobile/desktop OS built on the CodeOS kernel       ║
# ╚══════════════════════════════════════════════════════════════╝

CC := x86_64-elf-gcc
AR := x86_64-elf-ar
ARFLAGS := rcs
OBJCOPY := x86_64-elf-objcopy
MKDIR := mkdir -p
CP := cp

BUILD := build
KERNEL_DIR := ../kernel
ISO_DIR := build/iso
ISO_ROOT := $(ISO_DIR)/iso_root

CFLAGS := -std=c99 -Wall -Wextra -pedantic -g
CPPFLAGS := -I. -Igui -Iapps -Ibrowser -I../qt6/panels -I../pkgs/core/panels/src -I../pkgs/core/openssl/freestd -I$(KERNEL_DIR)/kernel -I../qt6/posixstubs/include -I../qt6/posixstubs/include/sys -I../qt6/sysroot/usr/include -D_GNU_SOURCE
# Locate libgcc from the cross compiler instead of hardcoding a versioned
# path (e.g. /usr/lib/gcc/x86_64-elf/16.2.0) that breaks on other machines.
LD_LIBGCC_DIR := $(dir $(shell $(CC) -print-libgcc-file-name 2>/dev/null))
LDFLAGS := -L../qt6/sysroot/usr/lib -L$(LD_LIBGCC_DIR) -lgcc
LDLIBS := -lm
RUST_TOOLCHAIN := $(HOME)/.rustup/toolchains/1.92.0-x86_64-unknown-linux-gnu/bin

# ── Sources ──

GUI_SRC := $(wildcard gui/*.c)
APP_SRC := $(wildcard apps/*.c)
ZIRCAPP_SRC := $(wildcard zircapp/*.c)
CORE_SRC := $(wildcard *.c)
BROWSER_SRC := browser/ow_bridge.c
SHIM_SRC := browser/ow_html_shim.c
NET_SRC := browser/net_shim.c
INIT_SRC := zircon_init.c
STUBS_SRC := stubs.c
CRT0_SRC := ../pkgs/core/lib-c/src/crt0.S

SRC := $(CORE_SRC) $(GUI_SRC) $(APP_SRC) $(ZIRCAPP_SRC)
ZIRCAPP_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(ZIRCAPP_SRC))
BROWSER_OBJ := $(BUILD)/browser_ow_bridge.o
OW_HTML_OBJ := $(BUILD)/browser_ow_html_shim.o
NET_OBJ := $(BUILD)/browser_net_shim.o
RUST_LIB := ../kernel/kernel/rust_ow/target/x86_64-unknown-none/release/libow_http.a
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
STUBS_OBJ := $(BUILD)/stubs.o
CRT0_OBJ := $(BUILD)/crt0.o
LIBC_OBJ := $(STUBS_OBJ) $(CRT0_OBJ)
DEP := $(OBJ:.o=.d) $(STUBS_OBJ:.o=.d) $(CRT0_OBJ:.o=.d) $(BROWSER_OBJ:.o=.d) $(OW_HTML_OBJ:.o=.d) $(NET_OBJ:.o=.d)

# ── Targets ──

ZIRCON_KERNEL := $(KERNEL_DIR)/codeos-1-kernel.bin
ZIRCON_INIT := $(BUILD)/zircon_init
ZIRCON_COMPOSITOR := $(BUILD)/zircond
ZIRCON_LIB := $(BUILD)/libzircon.a
ZIRCON_APP_LIB := $(BUILD)/libzircapp.a
ZIRCON_ELF := $(BUILD)/zircon.elf
ZIRCON_ISO := $(BUILD)/zircon.iso

# Limine stage-3 image. Do NOT use $(KERNEL_DIR)/bootloader/limine-bios.sys:
# that is a stale 351944-byte copy which does not match limine-bios-cd.bin,
# and limine rejects it at boot with "limine-bios.sys size incorrect" /
# "PANIC: Failed to load stage 3". The matching 302836-byte file is the one
# CodeOS's own working ISO ships, from its iso_root.
LIMINE_BIOS_SYS := $(KERNEL_DIR)/bootloader/iso_root/boot/limine-bios.sys
LIMINE_DEPLOY := $(KERNEL_DIR)/bootloader/limine-deploy

.PHONY: all clean debug release run iso initramfs zircon-init

all: $(ZIRCON_LIB) $(ZIRCON_APP_LIB) $(ZIRCON_COMPOSITOR) $(ZIRCON_INIT) $(ZIRCON_ELF)

debug: CFLAGS += -O0 -DDEBUG
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: all

# ── Rust library (OpenWeb) ──

$(RUST_LIB):
	$(MKDIR) $(dir $@)
	cd $(KERNEL_DIR)/rust_ow && PATH="$(RUST_TOOLCHAIN):$$PATH" cargo build --target x86_64-unknown-none --release

# ── Library ──

$(ZIRCON_LIB): $(OBJ) $(BROWSER_OBJ) $(OW_HTML_OBJ) $(NET_OBJ)
	$(MKDIR) $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

# ── ZircApp framework library ──
# The framework sources are also compiled straight into zircond/libzircon.a
# (they are in SRC); this archive is the standalone, linkable deliverable.
$(ZIRCON_APP_LIB): $(ZIRCAPP_OBJ)
	$(MKDIR) $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

# ── Browser & OpenWeb bridge ──

$(BROWSER_OBJ): $(BROWSER_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OW_HTML_OBJ): $(SHIM_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(NET_OBJ): $(NET_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Compositor (zircond) ──

$(ZIRCON_COMPOSITOR): $(filter-out $(BUILD)/zircon_init.o $(STUBS_OBJ), $(OBJ)) $(LIBC_OBJ) $(BROWSER_OBJ) $(OW_HTML_OBJ) $(NET_OBJ)
	$(CC) -nostartfiles -o $@ $(filter-out $(BUILD)/zircon_init.o $(STUBS_OBJ),$(OBJ)) $(LIBC_OBJ) $(BROWSER_OBJ) $(OW_HTML_OBJ) $(NET_OBJ) $(LDFLAGS) $(LDLIBS) $(RUST_LIB)

# ── Init process ──

$(ZIRCON_INIT): $(BUILD)/zircon_init.o $(LIBC_OBJ)
	$(CC) -nostartfiles -o $@ $< $(LIBC_OBJ) $(LDFLAGS) $(LDLIBS)

$(BUILD)/zircon_init.o: $(INIT_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Stubs ──

$(BUILD)/stubs.o: $(STUBS_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# ── CRT0 ──

$(BUILD)/crt0.o: $(CRT0_SRC)
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) -c $< -o $@

# ── Generic pattern rule ──

$(BUILD)/%.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# ── Zircon ELF (kernel + initramfs) ──

$(ZIRCON_ELF): $(ZIRCON_KERNEL) $(ZIRCON_INIT) $(ZIRCON_COMPOSITOR) $(ZIRCON_LIB) $(RUST_LIB)
	$(MKDIR) $(dir $@)
	$(CP) $(ZIRCON_KERNEL) $@

# ── Initramfs ──

initramfs: $(ZIRCON_INIT) $(ZIRCON_COMPOSITOR)
	$(MKDIR) $(ISO_ROOT)/sbin
	$(MKDIR) $(ISO_ROOT)/boot
	$(CP) $(ZIRCON_INIT) $(ISO_ROOT)/sbin/zircon_init
	$(CP) $(ZIRCON_COMPOSITOR) $(ISO_ROOT)/sbin/zircond
	$(CP) $(ZIRCON_KERNEL) $(ISO_ROOT)/boot/zircon-kernel.bin
	# Create init script
	echo '#!/bin/sh' > $(ISO_ROOT)/init
	echo 'mount -t proc none /proc' >> $(ISO_ROOT)/init
	echo 'mount -t sysfs none /sys' >> $(ISO_ROOT)/init
	echo 'echo "Zircon OS v1.0.0"' >> $(ISO_ROOT)/init
	echo '/sbin/zircon_init' >> $(ISO_ROOT)/init
	chmod +x $(ISO_ROOT)/init

# ── ISO ──

iso: initramfs
	$(MKDIR) $(ISO_DIR)
	$(MKDIR) $(ISO_ROOT)/boot
	$(MKDIR) $(ISO_ROOT)/sbin
	$(CP) $(ZIRCON_INIT) $(ISO_ROOT)/sbin/zircon_init
	$(CP) $(ZIRCON_COMPOSITOR) $(ISO_ROOT)/sbin/zircond
	$(CP) $(ZIRCON_KERNEL) $(ISO_ROOT)/boot/zircon-kernel.bin
	$(CP) $(KERNEL_DIR)/codeos-1.0.iso $(ISO_ROOT)/ 2>/dev/null || true
	# Create init script
	echo '#!/bin/sh' > $(ISO_ROOT)/init
	echo 'mount -t proc none /proc' >> $(ISO_ROOT)/init
	echo 'mount -t sysfs none /sys' >> $(ISO_ROOT)/init
	echo 'echo "Zircon OS v1.0.0"' >> $(ISO_ROOT)/init
	echo '/sbin/zircon_init' >> $(ISO_ROOT)/init
	chmod +x $(ISO_ROOT)/init
	# Copy limine bootloader files. These are the El Torito BIOS/UEFI images
	# plus the stage-3 image limine loads after its own stage 2.
	$(MKDIR) $(ISO_ROOT)/boot/limine
	$(CP) $(KERNEL_DIR)/bootloader/limine-bios-cd.bin $(ISO_ROOT)/boot/
	$(CP) $(KERNEL_DIR)/bootloader/limine-uefi-cd.bin $(ISO_ROOT)/boot/
	$(CP) $(LIMINE_BIOS_SYS) $(ISO_ROOT)/boot/
	# Zircon's OWN limine config, not the kernel's. The kernel config's
	# default_entry is /CodeOS, whose kernel_path (codeos-1-kernel.bin) is
	# absent from this image, so the countdown would land on a PANIC.
	$(CP) bootloader/limine.conf $(ISO_ROOT)/boot/limine.conf
	# Zircon's limine.conf sets `wallpaper: boot():/boot/splash.png`.
	$(CP) $(KERNEL_DIR)/bootloader/splash.png $(ISO_ROOT)/boot/
	# Build the ISO. These flags mirror the kernel's working codeos-1.0.iso
	# rule (kernel/Makefile:479). The previous form here passed
	# -c boot/boot.cat / -eltorito-alt-boot / -eltorito-platform 0xef /
	# -isohybrid-mbr, which libisofs rejects ("Invalid image size 3072 Kb.
	# Must be one of 1.2, 1.44or 2.88 Mb"); the 2>/dev/null hid that and a
	# broken fallback silently produced an unbootable image.
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

# ── Run ──

# The kernel cmdline lives in the limine config, not on the QEMU command line:
# -append is rejected without -kernel ("-append only allowed with -kernel
# option"), and this boots an ISO through limine.
run: $(ZIRCON_ISO)
	qemu-system-x86_64 \
		-cpu Haswell,+smep \
		-m 4096 \
		-smp 4 \
		-machine q35 \
		-cdrom $(ZIRCON_ISO) \
		-boot order=d \
		-serial stdio \
		-vga std
	# NOT +smap. The CodeOS kernel faults immediately once it turns SMAP on:
	#   !!! PF at RIP=0xffffffff80262dd9 CR2=0xffff800000241208 err=1 !!!
	# reproduced identically under KVM and TCG, on both -machine q35 and pc,
	# and independent of -m/-smp. SMEP alone is fine ("SMEP on, SMAP off" and
	# the kernel reaches "zircon: IPC bridge connected"). So this is a kernel
	# bug in the SMAP enablement path, worked around here rather than fixed --
	# re-enable +smap once the kernel side is sorted.

# ── Host tests (no kernel, no boot) ──
# Phase 1 drives the framework itself (tests/run.sh); Phase 2 drives the
# windowed host's WM encoder and event pump (tests/host/run.sh).  Both
# build the real sources against shims.  Run the second with ZC_CONTROL=1
# to prove the test is sensitive to the record-boundary logic.
zircapp-tests:
	sh zircapp/tests/run.sh
	sh zircapp/tests/host/run.sh

.PHONY: zircapp-tests

# ── Clean ──

clean:
	rm -rf $(BUILD)
	rm -f $(ZIRCON_LIB) $(ZIRCON_APP_LIB) $(ZIRCON_COMPOSITOR) $(ZIRCON_INIT) \
	      $(ZIRCON_ELF) $(ZIRCON_ISO)

-include $(DEP)
