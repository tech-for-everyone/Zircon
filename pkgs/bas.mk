# bas programs for Zircon.
#
# .ba sources under pkgs/core/bas/src/ are cross-compiled into real Zircon
# userland ELFs using pkgs/core/bas/sdk/targets/zircon.target, and land in
# $(ZROOT)/build/bin as bas-<stem>.
#
# bas is an OPTIONAL sibling checkout (CodeOS-Comunity/bas), the same treatment
# the OpenWeb tree gets.  With no checkout this target prints a line and succeeds
# -- it must never be able to fail a Zircon build.
#
# Included from Zircon/Makefile, which owns the image link.

ZROOT    ?= $(shell d=$(CURDIR); while [ ! -f "$$d/pkgs/pkgs.mk" ]; do \
	d=$$(dirname "$$d"); [ "$$d" = "/" ] && break; done; echo "$$d")

# bas is a sibling of the CodeOS tree, so from Zircon/ that is ../../bas; the
# ../bas form covers a Zircon checked out on its own next to it.  No absolute
# path is listed: a machine-specific default would make this file silently
# non-portable, which is exactly the failure the other optional checkouts
# (OpenWeb) are written to avoid.
BAS_DIR     ?= $(firstword $(wildcard $(ZROOT)/../../bas $(ZROOT)/../bas))
BAS_CC      = $(BAS_DIR)/compiler/target/debug/bas
BAS_SRC_DIR = $(ZROOT)/../pkgs/core/bas/src
BAS_SDK_DIR = $(ZROOT)/../pkgs/core/bas/sdk
BAS_OUT_DIR = $(BUILD)/bin

BAS_NAMES = $(notdir $(basename $(wildcard $(BAS_SRC_DIR)/*.ba)))
BAS_PROGS = $(addprefix bas-,$(BAS_NAMES))
BAS_BINS  = $(addprefix $(BAS_OUT_DIR)/,$(BAS_PROGS))

.PHONY: bas_progs
bas_progs:
	@if [ -z "$(strip $(BAS_DIR))" ] || [ ! -f "$(BAS_CC)" ]; then \
	    printf 'bas: no compiler at %s; bas programs omitted\n' '$(BAS_CC)'; \
	else \
	    mkdir -p $(BAS_OUT_DIR); \
	    set -e; for b in $(BAS_NAMES); do \
	        echo "  BAS  $$b.ba -> bas-$$b (zircon)"; \
	        $(BAS_CC) build $(BAS_SRC_DIR)/$$b.ba \
	            --target zircon \
	            --target-dir $(BAS_SDK_DIR)/targets \
	            --sysroot $(ZROOT)/.. \
	            -o $(BAS_OUT_DIR)/bas-$$b; \
	    done; \
	fi