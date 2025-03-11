# SAMPLERATE
SAMPLERATE_VERSION := 0.1.9
SAMPLERATE_URL := http://www.mega-nerd.com/SRC/libsamplerate-$(SAMPLERATE_VERSION).tar.gz

ifdef GPL
PKGS += samplerate
endif
ifeq ($(call need_pkg,"samplerate"),)
PKGS_FOUND += samplerate
endif

ifdef HAVE_QNX
# qcc enables -pipe by default and doesn't actually understand the '-pipe' option
SAMPLERATE_CONF := --disable-gcc-pipe
endif

$(TARBALLS)/libsamplerate-$(SAMPLERATE_VERSION).tar.gz:
	$(call download_pkg,$(SAMPLERATE_URL),samplerate)

.sum-samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz

samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz .sum-samplerate
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub Cfg
	$(MOVE)

.samplerate: samplerate
	$(REQUIRE_GPL)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SAMPLERATE_CONF)
	cd $< && $(MAKE) -C src install && $(MAKE) install-data
	touch $@
