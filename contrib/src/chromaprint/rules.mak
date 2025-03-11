# chromaprint

CHROMAPRINT_VERSION := 1.4.2
CHROMAPRINT_URL := $(GITHUB)/acoustid/chromaprint/releases/download/v$(CHROMAPRINT_VERSION)/chromaprint-$(CHROMAPRINT_VERSION).tar.gz

PKGS += chromaprint
ifeq ($(call need_pkg,"libchromaprint"),)
PKGS_FOUND += chromaprint
endif

$(TARBALLS)/chromaprint-$(CHROMAPRINT_VERSION).tar.gz:
	$(call download_pkg,$(CHROMAPRINT_URL),chromaprint)

.sum-chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz

chromaprint: chromaprint-$(CHROMAPRINT_VERSION).tar.gz .sum-chromaprint
	$(UNPACK)
	$(APPLY) $(SRC)/chromaprint/linklibs.patch
ifdef HAVE_QNX
	$(APPLY) $(SRC)/chromaprint/0001-qnx-enable-PIC.patch
endif
	$(MOVE)

DEPS_chromaprint = ffmpeg $(DEPS_ffmpeg)

ifdef HAVE_QNX
CHROMAPRINT_CMAKEARGS := -DBUILD_ENABLE_PIC=ON
endif

.chromaprint: chromaprint .ffmpeg toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) $(CHROMAPRINT_CMAKEARGS)
	cd $< && $(CMAKEBUILD) . --target install
	touch $@
