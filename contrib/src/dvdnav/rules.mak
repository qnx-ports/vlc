# DVDNAV

LIBDVDNAV_VERSION := 6.1.1
LIBDVDNAV_URL := $(VIDEOLAN)/libdvdnav/$(LIBDVDNAV_VERSION)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdnav
endif
endif
ifeq ($(call need_pkg,"dvdnav > 5.0.3"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2:
	$(call download,$(LIBDVDNAV_URL))

.sum-dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2 .sum-dvdnav
	$(UNPACK)
	$(APPLY) $(SRC)/dvdnav/0001-configure-don-t-use-ms-style-packing.patch
	# turn asserts/exit into silent discard
	$(APPLY) $(SRC)/dvdnav/0001-play-avoid-assert-and-exit-and-bogus-PG-link.patch
	$(APPLY) $(SRC)/dvdnav/0002-play-avoid-assert-and-exit-and-bogus-Cell-link.patch
	$(call pkg_static,"misc/dvdnav.pc.in")
	$(MOVE)

DEPS_dvdnav = dvdread $(DEPS_dvdread)

.dvdnav: dvdnav
	$(REQUIRE_GPL)
	$(RECONF) -I m4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-examples
	cd $< && $(MAKE) install
	touch $@
