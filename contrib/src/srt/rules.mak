# srt

SRT_VERSION := 1.4.4
SRT_URL := $(GITHUB)/Haivision/srt/archive/v$(SRT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += srt
endif

ifeq ($(call need_pkg,"srt >= 1.3.1"),)
PKGS_FOUND += srt
endif

DEPS_srt = gnutls $(DEPS_gnutls)
ifdef HAVE_WIN32
DEPS_srt += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/srt-$(SRT_VERSION).tar.gz:
	$(call download_pkg,$(SRT_URL),srt)

.sum-srt: srt-$(SRT_VERSION).tar.gz

srt: srt-$(SRT_VERSION).tar.gz .sum-srt
	$(UNPACK)
	$(APPLY) $(SRC)/srt/0001-core-ifdef-MSG_TRUNC-nixes-fix.patch
ifdef HAVE_QNX
	$(APPLY) $(SRC)/srt/0001-qnx-support.patch
	$(APPLY) $(SRC)/srt/0002-qnx-static-pic.patch
endif
	$(call pkg_static,"scripts/srt.pc.in")
	mv srt-$(SRT_VERSION) $@ && touch $@

ifdef HAVE_QNX
CMAKE_EXTRA_ARGS += "-DENABLE_STATIC_PIC=ON"
endif

.srt: srt toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_SHARED=OFF -DUSE_GNUTLS=ON -DENABLE_CXX11=OFF -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_INSTALL_INCLUDEDIR=include \
		$(CMAKE_EXTRA_ARGS)
	cd $< && $(CMAKEBUILD) . --target install
	touch $@
