# matroska

MATROSKA_VERSION := 1.6.3
MATROSKA_URL := http://dl.matroska.org/downloads/libmatroska/libmatroska-$(MATROSKA_VERSION).tar.xz

PKGS += matroska

ifeq ($(call need_pkg,"libmatroska"),)
PKGS_FOUND += matroska
endif

DEPS_matroska = ebml $(DEPS_ebml)

$(TARBALLS)/libmatroska-$(MATROSKA_VERSION).tar.xz:
	$(call download_pkg,$(MATROSKA_URL),matroska)

.sum-matroska: libmatroska-$(MATROSKA_VERSION).tar.xz

matroska: libmatroska-$(MATROSKA_VERSION).tar.xz .sum-matroska
	$(UNPACK)
ifdef HAVE_QNX
	$(APPLY) $(SRC)/matroska/0001-qnx-enable-pic.patch
endif
	$(call pkg_static,"libmatroska.pc.in")
	$(MOVE)

.matroska: matroska toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE)
	cd $< && $(CMAKEBUILD) . --target install
	touch $@
