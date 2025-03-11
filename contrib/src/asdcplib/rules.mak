# asdcplib

ASDCPLIB_VERSION := 2.7.19

ASDCPLIB_URL := http://download.cinecert.com/asdcplib/asdcplib-$(ASDCPLIB_VERSION).tar.gz

ifndef HAVE_IOS
ifndef HAVE_ANDROID
ifndef HAVE_WINSTORE
PKGS += asdcplib
endif
endif
endif

ifeq ($(call need_pkg,"asdcplib >= 1.12"),)
PKGS_FOUND += asdcplib
endif

ASDCPLIB_CXXFLAGS := $(CXXFLAGS)
ifdef HAVE_QNX
# QNX uses GCC and libc++, and libc++ only supports C++03 (and earlier)
# when compiled with clang. Work around this by bumping the C++ version.
ASDCPLIB_CXXFLAGS += -std=gnu++11
else
ASDCPLIB_CXXFLAGS += -std=gnu++98
endif

$(TARBALLS)/asdcplib-$(ASDCPLIB_VERSION).tar.gz:
	$(call download_pkg,$(ASDCPLIB_URL),asdcplib)

.sum-asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz

asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz .sum-asdcplib
	$(UNPACK)
	$(APPLY) $(SRC)/asdcplib/port-to-nettle.patch
	$(APPLY) $(SRC)/asdcplib/static-programs.patch
	$(APPLY) $(SRC)/asdcplib/adding-pkg-config-file.patch
	$(APPLY) $(SRC)/asdcplib/win32-cross-compilation.patch
	$(APPLY) $(SRC)/asdcplib/win32-dirent.patch
	$(APPLY) $(SRC)/asdcplib/0001-Remove-a-broken-unused-template-class.patch
ifdef HAVE_QNX
	$(APPLY) $(SRC)/asdcplib/0001-qnx-support.patch
endif
	$(MOVE)

DEPS_asdcplib = nettle $(DEPS_nettle)

.asdcplib: asdcplib
	$(RECONF)
	cd $< && $(HOSTVARS) CXXFLAGS="$(ASDCPLIB_CXXFLAGS)" ./configure $(HOSTCONF) --enable-freedist --enable-dev-headers --with-nettle=$(PREFIX)
	cd $< && $(MAKE) install
	touch $@
