# x265

#X265_GITURL := https://github.com/videolan/x265
X265_VERSION := 2.9
X265_SNAPURL := https://bitbucket.org/multicoreware/x265_git/downloads/x265_$(X265_VERSION).tar.gz

ifdef BUILD_ENCODERS
ifdef GPL
PKGS += x265
endif
endif

ifeq ($(call need_pkg,"x265 >= 0.6"),)
PKGS_FOUND += x265
endif

ifdef HAVE_QNX
ENABLE_PIC := -DENABLE_PIC=ON
endif

$(TARBALLS)/x265-git.tar.xz:
	$(call download_git,$(X265_GITURL))

$(TARBALLS)/x265_$(X265_VERSION).tar.gz:
	$(call download_pkg,$(X265_SNAPURL),x265)

.sum-x265: x265_$(X265_VERSION).tar.gz

x265: x265_$(X265_VERSION).tar.gz .sum-x265
	$(UNPACK)
	$(APPLY) $(SRC)/x265/x265-ldl-linking.patch
	$(APPLY) $(SRC)/x265/x265-no-pdb-install.patch
	$(APPLY) $(SRC)/x265/x265-enable-detect512.patch
ifdef HAVE_QNX
	$(APPLY) $(SRC)/x265/0001-qnx-use-gnu++11.patch
	$(APPLY) $(SRC)/x265/0002-qnx-dont-link-libcS.patch
	$(APPLY) $(SRC)/x265/0003-qnx-get-compiler-version.patch
endif
	$(call pkg_static,"source/x265.pc.in")
ifndef HAVE_WIN32
ifndef HAVE_QNX
	$(APPLY) $(SRC)/x265/x265-pkg-libs.patch
endif
endif
	$(MOVE)

.x265: x265 toolchain.cmake
	$(REQUIRE_GPL)
	cd $</source && $(HOSTVARS_PIC) $(CMAKE) -DENABLE_SHARED=OFF -DCMAKE_SYSTEM_PROCESSOR=$(ARCH) -DENABLE_CLI=OFF $(ENABLE_PIC)
	cd $< && $(CMAKEBUILD) source --target install
	sed -e s/'[^ ]*clang_rt[^ ]*'//g -i.orig "$(PREFIX)/lib/pkgconfig/x265.pc"
	touch $@
