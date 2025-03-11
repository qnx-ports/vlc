# NFS
NFS_VERSION := 5.0.1
NFS_URL := $(GITHUB)/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

PKGS += nfs
ifeq ($(call need_pkg,"libnfs >= 1.10"),)
PKGS_FOUND += nfs
endif

$(TARBALLS)/libnfs-$(NFS_VERSION).tar.gz:
	$(call download_pkg,$(NFS_URL),nfs)

.sum-nfs: libnfs-$(NFS_VERSION).tar.gz

nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	mv libnfs-libnfs-$(NFS_VERSION) libnfs-$(NFS_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/nfs/qnx_use_sys_time.patch
	$(APPLY) $(SRC)/nfs/qnx_three_arg_makedev.patch
	$(APPLY) $(SRC)/nfs/qnx_use_getsockname.patch
	$(MOVE)

.nfs: nfs
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-utils --disable-werror $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
