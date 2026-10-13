# ╔══════════════════════════════════════════════════════════════╗
# ║  Zircon — package orchestrator                                 ║
# ╚══════════════════════════════════════════════════════════════╝
#
# Every package owns its own Makefile; this one only discovers them (any
# directory that carries a manifest.json) and walks the list, so adding a
# package means dropping in a manifest + Makefile, nothing more.  The
# top-level Zircon/Makefile links the results into the images.
#
#   make -C pkgs            build every package
#   make -C pkgs clean      clean every package
#   make -C pkgs list       print the discovered packages
#
# template/ is a copy-me skeleton and is deliberately not built.

PKG_MANIFESTS := $(shell find . -name manifest.json -not -path './template/*' | sort)
PKG_DIRS      := $(patsubst %/manifest.json,%,$(PKG_MANIFESTS))

.PHONY: all clean list $(PKG_DIRS)
all: $(PKG_DIRS)

$(PKG_DIRS):
	@$(MAKE) -s -C $@ all

list:
	@printf '%s\n' $(PKG_DIRS)

clean:
	@for d in $(PKG_DIRS); do $(MAKE) -s -C $$d clean; done
