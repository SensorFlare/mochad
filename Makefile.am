bin_PROGRAMS = mochad
mochad_SOURCES = mochad.c decode.c encode.c global.c x10state.c x10_write.c \
		 decode.h encode.h global.h x10state.h x10_write.h
EXTRA_DIST = udev/91-usb-x10-controllers.rules hotplug2/20-usb-x10 hotplug2/mochad \
	     cgi/x10.pl cgi/netcat.pl cgi/getsensors.pl cgi/cgi-lib.pl \
	     apps/mochamon.pl apps/simplemon.pl apps/bash.sh \
	     apps/rfsectopl3.pl apps/x10-tk.py

install-exec-hook:
	if test -d /etc/udev/rules.d ; then \
		cp $(abs_top_srcdir)/udev/91-usb-x10-controllers.rules /etc/udev/rules.d ; \
	fi ;
	if test -d /etc/hotplug.d/usb ; then \
		cp $(abs_top_srcdir)/hotplug2/20-usb-x10 /etc/hotplug.d/usb ; \
		cp $(abs_top_srcdir)/hotplug2/mochad /etc/init.d ; \
	fi ;
