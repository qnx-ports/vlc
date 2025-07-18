# Qt

QT_VERSION := 5.6.3
QT_URL := https://download.qt.io/archive/qt/5.6/$(QT_VERSION)/submodules/qtbase-opensource-src-$(QT_VERSION).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
endif

ifeq ($(call need_pkg,"Qt5Core Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif

$(TARBALLS)/qt-$(QT_VERSION).tar.xz:
	$(call download_pkg,$(QT_URL),qt)

.sum-qt: qt-$(QT_VERSION).tar.xz

qt: qt-$(QT_VERSION).tar.xz .sum-qt
	$(UNPACK)
	mv qtbase-opensource-src-$(QT_VERSION) qt-$(QT_VERSION)
	$(APPLY) $(SRC)/qt/0001-Windows-QPA-Reimplement-calculation-of-window-frames_56.patch
	$(APPLY) $(SRC)/qt/0002-Windows-QPA-Use-new-EnableNonClientDpiScaling-for-Wi_56.patch
	$(APPLY) $(SRC)/qt/0003-QPA-prefer-lower-value-when-rounding-fractional-scaling.patch
	$(APPLY) $(SRC)/qt/0004-qmake-don-t-limit-command-line-length-when-not-actua.patch
	$(APPLY) $(SRC)/qt/0005-harfbuzz-Fix-building-for-win64-with-clang.patch
	$(APPLY) $(SRC)/qt/0006-moc-Initialize-staticMetaObject-with-the-highest-use.patch
	$(APPLY) $(SRC)/qt/0007-Only-define-QT_FASTCALL-on-x86_32.patch
	$(APPLY) $(SRC)/qt/0008-Skip-arm-pixman-drawhelpers-on-windows-just-like-on-.patch
	$(APPLY) $(SRC)/qt/0009-mkspecs-Add-a-win32-clang-g-mkspec-for-clang-targeti.patch
	$(APPLY) $(SRC)/qt/0010-Add-the-QT_HAS_xxx-macros-for-post-C-11-feature-test.patch
	$(APPLY) $(SRC)/qt/0011-qCount-Leading-Trailing-ZeroBits-Use-__builtin_clzs-.patch
	$(APPLY) $(SRC)/qt/0012-Remove-_bit_scan_-forward-reverse.patch
	$(APPLY) $(SRC)/qt/0013-qsimd-Fix-compilation-with-trunk-clang-for-mingw.patch
	$(APPLY) $(SRC)/qt/0014-QtTest-compile-in-C-17-mode-no-more-std-unary_functi.patch
	$(APPLY) $(SRC)/qt/0015-foreach-remove-implementations-not-using-decltype.patch
	$(APPLY) $(SRC)/qt/0016-Replace-custom-type-traits-with-std-one-s.patch
	$(APPLY) $(SRC)/qt/0017-Rename-QtPrivate-is_-un-signed-to-QtPrivate-Is-Un-si.patch
	$(APPLY) $(SRC)/qt/0018-Remove-qtypetraits.h-s-contents-altogether.patch
	$(APPLY) $(SRC)/qt/0019-QFileSystemEngine-only-define-FILE_ID_INFO-for-build.patch
	$(APPLY) $(SRC)/qt/systray-no-sound.patch
	# fix forcing the WINVER/_WIN32_WINNT version without NTDDI_VERSION
	sed -i.orig -e "s/DEFINES += WINVER/DEFINES += NTDDI_VERSION=0x06000000 WINVER/" "$(UNPACK_DIR)/src/network/kernel/kernel.pri"
	# TOUCHINPUT is properly defined in mingw since v4
	sed -i.orig -e "s/defined(Q_CC_MINGW) || !defined(TOUCHEVENTF_MOVE)/!defined(TOUCHEVENTF_MOVE)/" "$(UNPACK_DIR)/src/plugins/platforms/windows/qtwindows_additional.h"
	$(MOVE)

ifdef HAVE_MACOSX
QT_PLATFORM := -platform darwin-g++
endif
ifdef HAVE_WIN32
# filter out the contrib includes as Qt doesn't ike pthread-GC2 headers
QT_VARS := CFLAGS="$(shell echo $$CFLAGS | sed 's@ -I$$(PREFIX)/include@@g')" \
         CXXFLAGS="$(shell echo $$CXXFLAGS | sed 's@ -I$$(PREFIX)/include@@g')" \
         LDFLAGS="-L$(PREFIX)/lib $(EXTRA_LDFLAGS)"
ifdef HAVE_CLANG
QT_SPEC := win32-clang-g++
else
QT_SPEC := win32-g++
endif
QT_PLATFORM := -xplatform $(QT_SPEC) -device-option CROSS_COMPILE=$(HOST)-
endif

QT_CONFIG := -static -opensource -confirm-license -no-pkg-config \
	-no-sql-sqlite -no-gif -qt-libjpeg -no-openssl -no-opengl -no-dbus \
	-no-qml-debug -no-audio-backend -no-sql-odbc -no-pch \
	-no-compile-examples -nomake examples

QT_CONFIG += -release

.qt: qt
	cd $< && $(QT_VARS) ./configure $(QT_PLATFORM) $(QT_CONFIG) -prefix $(PREFIX)
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-corelib-install_subtargets sub-gui-install_subtargets sub-widgets-install_subtargets sub-platformsupport-install_subtargets sub-zlib-install_subtargets sub-bootstrap-install_subtargets
	# Install tools
	cd $< && $(MAKE) -C src sub-moc-install_subtargets sub-rcc-install_subtargets sub-uic-install_subtargets
	# Install plugins
	cd $< && $(MAKE) -C src/plugins sub-platforms-install_subtargets
	mv $(PREFIX)/plugins/platforms/libqwindows.a $(PREFIX)/lib/ && rm -rf $(PREFIX)/plugins
	# Move includes to match what VLC expects
	mkdir -p $(PREFIX)/include/QtGui/qpa
	cp $(PREFIX)/include/QtGui/$(QT_VERSION)/QtGui/qpa/qplatformnativeinterface.h $(PREFIX)/include/QtGui/qpa
	# Clean Qt mess
	rm -rf $(PREFIX)/lib/libQt5Bootstrap* $</lib/libQt5Bootstrap*
	# Fix .pc files to remove debug version (d)
	cd $(PREFIX)/lib/pkgconfig; for i in Qt5Core.pc Qt5Gui.pc Qt5Widgets.pc; do sed -i -e 's/d\.a/.a/g' -e 's/d $$/ /' $$i; done
	# Fix Qt5Gui.pc file to include qwindows (QWindowsIntegrationPlugin) and Qt5Platform Support
	cd $(PREFIX)/lib/pkgconfig; sed -i -e 's/ -lQt5Gui/ -lqwindows -lQt5PlatformSupport -lQt5Gui/g' Qt5Gui.pc
ifdef HAVE_CROSS_COMPILE
	# Building Qt build tools for Xcompilation
	cd $</include/QtCore; $(LN_S)f $(QT_VERSION)/QtCore/private private
	cd $<; $(MAKE) -C qmake
	cd $<; $(MAKE) install_qmake install_mkspecs
	cd $</src/tools; \
	for i in bootstrap uic rcc moc; \
		do (cd $$i; echo $$i && ../../../bin/qmake -spec $(QT_SPEC) && $(MAKE) clean && $(MAKE) CC=$(HOST)-gcc CXX=$(HOST)-g++ LINKER=$(HOST)-g++ LIB="$(HOST)-ar -rc" && $(MAKE) install); \
	done
endif
	touch $@
