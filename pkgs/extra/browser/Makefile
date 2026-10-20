# Per-package build.  Sources live in src/; the toolchain and include paths
# come from pkgs/pkgs.mk via pkgs/package.mk (see pkgs/README.md).
PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")/pkgs/package.mk

# net_shim.c is compiled WITHOUT the -mno-sse the rest of the userland uses.
#
# The file provides fma(), and a double-returning function cannot be compiled
# with SSE disabled: "SSE register return with SSE disabled".  The symbol is
# here only to satisfy the Rust linker's expectations -- nothing calls it --
# so it is the one file in the userland that keeps SSE.
#
# The rest of the userland is -mno-sse because the kernel runs user programs
# without an FXSAVE area: an SSE instruction in user mode raises #UD there,
# which is how zircon_init died at its first memset ("!!! UD (vec 6) at
# RIP=0x401363", an xorpd %xmm0,%xmm0).
$(PKG_BUILD)/net_shim.o: PKG_CFLAGS := $(filter-out -mno-sse -mno-sse2,$(PKG_CFLAGS))
