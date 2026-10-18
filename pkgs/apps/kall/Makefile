# Per-package build.  Sources live in src/; the toolchain and include paths
# come from pkgs/pkgs.mk via pkgs/package.mk (see pkgs/README.md).
PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")/pkgs/package.mk
