PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
PKG_CFLAGS += -I$(ZROOT)/../include
include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")/pkgs/package.mk
