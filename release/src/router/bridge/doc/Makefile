
DESTDIR=
KERNEL_HEADERS=-I/home/paldier/asuswrt/release/src-ra-4300/linux/linux-2.6.36.x/include

INSTALL=install -s

prefix=
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=${prefix}/man

SUBDIRS=libbridge brctl

all:

clean:

install:
	mkdir -p $(DESTDIR)$(mandir)/man8
	install -m 644 brctl.8 $(DESTDIR)$(mandir)/man8
