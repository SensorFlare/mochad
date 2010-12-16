include $(TOPDIR)/rules.mk

PKG_NAME:=mochad
PKG_VERSION:=0.1.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://sourceforge.net/projects/mochad/files
PKG_MD5SUM:=5d80a6532d6af73f822cc31c322b2b89

PKG_BUILD_DEPENDS:=+libusb-1.0
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk

define Package/mochad
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=X10 USB controller driver
	URL:=http://sourceforge.net/projects/mochad
	DEPENDS:=+libusb-1.0 +usbutils
endef

define Package/mochad/description
X10 USB controller driver X10 CM15A and CM19A USB device driver with 
TCP socket gateway.
endef

# uses GNU configure

define Package/mochad/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mochad $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hotplug2/mochad		$(1)/etc/init.d/
	$(INSTALL_DIR) $(1)/etc/hotplug.d/usb
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/hotplug2/20-usb-x10	$(1)/etc/hotplug.d/usb
endef

$(eval $(call BuildPackage,mochad))
