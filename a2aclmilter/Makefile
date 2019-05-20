CFLAGS = -O0 -g -W -Wall -Wextra -Wpedantic

INSTALL_BIN 	= install -m 0555
INSTALL_MAN	= install -m 0444

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/man

a2aclmilter: util.o a2aclmilter.c
	${CC} ${CFLAGS} -Wno-unused-parameter -L/usr/local/lib \
	    -I/usr/local/include util.o a2aclmilter.c -la2id -la2acl -lmilter \
	    -pthread -o $@

util.o: util.c util.h
	${CC} ${CFLAGS} -c util.c

install: a2aclmilter.8 a2aclmilter
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man8
	$(INSTALL_BIN) a2aclmilter $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) a2aclmilter.8 $(DESTDIR)$(MANDIR)/man8

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/a2aclmilter
	rm -f $(DESTDIR)$(MANDIR)/man8/a2aclmilter.8

manhtml: ../doc/man/man.css a2aclmilter.8
	mandoc -T html -Ostyle=man.css a2aclmilter.8 > a2aclmilter.8.html

clean:
	rm -f a2aclmilter util.o a2aclmilter.8.html
