all: genext2fs
INSTALL=install

install:
	$(INSTALL) -d $(DESTDIR)/usr/bin/
	$(INSTALL) -m 755 genext2fs $(DESTDIR)/usr/bin/
	$(INSTALL) -d $(DESTDIR)/usr/share/man/man8/
	$(INSTALL) -m 644 genext2fs.8 $(DESTDIR)/usr/share/man/man8/

clean:
	-rm genext2fs

