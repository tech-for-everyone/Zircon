# ╔══════════════════════════════════════════════════════════════╗
# ║  Zircon — shared package recipe                                ║
# ╚══════════════════════════════════════════════════════════════╝
#
# Included by every package Makefile after it has set PKG_SRCS (and, when
# needed, extra PKG_CFLAGS).  It compiles $(PKG_SRCS) into $(PKG_BUILD) and
# archives them into $(PKG_LIB); the top-level Makefile links the images.
#
# A package Makefile therefore stays a handful of lines:
#
#     PKG_SRCS := $(wildcard src/*.c) $(wildcard src/*.S)
#     include $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
#         d=$$(dirname "$$d"); done; echo "$$d")/pkgs/package.mk

PKGS_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(PKGS_MK_DIR)pkgs.mk

PKG_OBJS := $(patsubst src/%.c,$(PKG_BUILD)/%.o,$(patsubst src/%.S,$(PKG_BUILD)/%.o,$(PKG_SRCS)))
PKG_DEPS := $(PKG_OBJS:.o=.d)

all: $(PKG_LIB)

$(PKG_LIB): $(PKG_OBJS)
	@$(MKDIR) $(@D)
	$(AR) $(ARFLAGS) $@ $^

$(PKG_BUILD)/%.o: src/%.c
	@$(MKDIR) $(@D)
	$(CC) $(CPPFLAGS) $(PKG_CFLAGS) -MMD -MP -c $< -o $@

$(PKG_BUILD)/%.o: src/%.S
	@$(MKDIR) $(@D)
	$(AS) $(CPPFLAGS) -c $< -o $@

-include $(PKG_DEPS)

.PHONY: all clean
clean:
	rm -rf $(PKG_BUILD)
