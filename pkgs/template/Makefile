# Template package Makefile.  Identical to every other package's: the only
# per-package input is the source list, and everything else is shared.
PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")/pkgs/package.mk
