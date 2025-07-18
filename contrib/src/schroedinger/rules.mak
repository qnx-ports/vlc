# schroedinger

SCHROEDINGER_VERSION := 1.0.11
SCHROEDINGER_URL := http://diracvideo.org/download/schroedinger/schroedinger-$(SCHROEDINGER_VERSION).tar.gz

PKGS += schroedinger
ifeq ($(call need_pkg,"schroedinger-1.0"),)
PKGS_FOUND += schroedinger
endif

$(TARBALLS)/schroedinger-$(SCHROEDINGER_VERSION).tar.gz:
	$(call download_pkg,$(SCHROEDINGER_URL),schroedinger)

.sum-schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz

schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz .sum-schroedinger
	$(UNPACK)
	$(APPLY) $(SRC)/schroedinger/schroedinger-notests.patch
	# disable orc compilation, the old compiler matches what was used to precompile
	$(APPLY) $(SRC)/schroedinger/schroedinger-disable-orcc.patch
	$(call pkg_static,"schroedinger.pc.in")
	$(MOVE)

DEPS_schroedinger = orc $(DEPS_orc)

SCHRODINGER_CONF := --with-thread=none --disable-gtk-doc

.schroedinger: schroedinger
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SCHRODINGER_CONF)
	cd $< && $(MAKE) install
	touch $@
