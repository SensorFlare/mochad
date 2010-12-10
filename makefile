# Copyright 2010 Brian Uechi <buasst@gmail.com>
#
# This file is part of mochad.
#
# mochad is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# mochad is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with mochad.  If not, see <http://www.gnu.org/licenses/>.

SHELL = /bin/bash

.SUFFIXES:
.SUFFIXES: .c .o

RELDIR=mochad-0.1.1
OBJS=mochad.o encode.o decode.o x10state.o x10_write.o global.o
SRCS=mochad.c global.c global.h encode.c encode.h decode.c decode.h \
	x10state.c x10state.h x10_write.c x10_write.h makefile COPYING \
	README mochamon.pl .gitignore cgi/
CFLAGS=-c -Wall -O2
LIBS=-lusb-1.0
LDFLAGS=$(LIBS)

ifndef $(TARGET)
    TARGET=native
endif

ifeq ($(TARGET), native)
CC=gcc
LD=$(CC) -o mochad
STRIP=strip
endif

ifeq ($(TARGET), dockstar)
ARCH=arm_v5te
ARCH2=arm
LIBVER=uClibc-0.9.31_eabi
LIBVER2=uclibcgnueabi
STAGING=/home/me/dockstar/trunk/staging_dir
TOOLBIN=$(STAGING)/toolchain-$(ARCH)_gcc-4.3.3+cs_$(LIBVER)/bin
TOOLINC=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/include
TOOLLIB=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/lib
CC="$(TOOLBIN)/$(ARCH2)-openwrt-linux-$(LIBVER2)-gcc" -I $(TOOLINC)
LD=$(CC) -o mochad  -L $(TOOLLIB)
STRIP="$(TOOLBIN)/$(ARCH2)-openwrt-linux-$(LIBVER2)-strip"
endif

ifeq ($(TARGET), nslu2)
ARCH=armeb_v5te
LIBVER=uClibc-0.9.30.1_eabi
STAGING=/home/me/nslu2/backfire/staging_dir
TOOLBIN=$(STAGING)/toolchain-$(ARCH)_gcc-4.3.3+cs_$(LIBVER)/usr/bin
TOOLINC=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/include
TOOLLIB=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/lib
CC="$(TOOLBIN)/armeb-openwrt-linux-gcc" -I $(TOOLINC)
LD=$(CC) -o mochad  -L $(TOOLLIB)
STRIP="$(TOOLBIN)/armeb-openwrt-linux-strip"
endif

ifeq ($(TARGET), g300)
ARCH=mips_r2
LIBVER=uClibc-0.9.30.1
STAGING=/home/me/g300/backfire/staging_dir
TOOLBIN=$(STAGING)/toolchain-$(ARCH)_gcc-4.3.3+cs_$(LIBVER)/usr/bin
TOOLINC=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/include
TOOLLIB=$(STAGING)/target-$(ARCH)_$(LIBVER)/usr/lib
CC="$(TOOLBIN)/mips-openwrt-linux-uclibc-gcc" -I $(TOOLINC)
LD=$(CC) -o mochad  -L $(TOOLLIB)
STRIP="$(TOOLBIN)/mips-openwrt-linux-uclibc-strip"
endif

ifeq ($(TARGET), chumby)
CC=arm-linux-gcc -I ~/libusb-1.0.8/install/include
LD=arm-linux-gcc -o mochad -L ~/libusb-1.0.8/install/lib
STRIP=arm-linux-strip
endif

%.o:	%.c 
	$(CC) $(CFLAGS) $< -o $@

mochad: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS)
	$(STRIP) $@

mochad.o: mochad.c global.h x10state.h x10_write.h encode.h decode.h

encode.o: encode.c global.h encode.h decode.h x10state.h x10_write.h

decode.o: decode.c global.h decode.h encode.h x10state.h x10_write.h

x10state.o: x10state.c global.h x10state.h decode.h

x10_write.o: x10_write.c global.h x10_write.h

global.o: global.c global.h

clean:
	rm $(OBJS) mochad

release:
	echo $(STRIP) $(RELDIR)
	if [ -d $(RELDIR) ] ; then rm -rf $(RELDIR) ; fi
	mkdir $(RELDIR)
	cp -r $(SRCS) $(RELDIR)
	tar czf $(RELDIR).tar.gz --exclude=*~ $(RELDIR)
