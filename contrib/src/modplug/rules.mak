# modplug

MODPLUG_VERSION := 0.8.9.0
MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.9.0"),)
PKGS_FOUND += modplug
endif

MODPLUG_CXXFLAGS := $(CXXFLAGS)
ifdef HAVE_QNX
# QNX uses GCC and libc++, and libc++ only supports C++03 (and earlier)
# when compiled with clang. Work around this by bumping the C++ version.
MODPLUG_CXXFLAGS += -std=gnu++11
else
MODPLUG_CXXFLAGS += -std=gnu++98
endif

$(TARBALLS)/libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(call download_pkg,$(MODPLUG_URL),modplug)

.sum-modplug: libmodplug-$(MODPLUG_VERSION).tar.gz

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz .sum-modplug
	$(UNPACK)
	$(APPLY) $(SRC)/modplug/modplug-win32-static.patch
	$(APPLY) $(SRC)/modplug/macosx-do-not-force-min-version.patch
	$(APPLY) $(SRC)/modplug/fix-endianness-check.diff
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/modplug/mac-use-c-stdlib.patch
endif
ifdef HAVE_QNX
	$(APPLY) $(SRC)/modplug/qnx-use-libc++.patch
endif
	$(call pkg_static,"libmodplug.pc.in")
	$(MOVE)

.modplug: libmodplug
	$(RECONF)
	cd $< && $(HOSTVARS) CXXFLAGS="$(MODPLUG_CXXFLAGS)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
