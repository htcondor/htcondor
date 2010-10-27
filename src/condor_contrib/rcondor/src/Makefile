prefix = /usr/local
bindir = $(prefix)/bin
libexecdir = $(prefix)/libexec
datarootdir = $(prefix)/share
docdir = $(datarootdir)/doc/rcondor

.PHONY: install
install:
# install directories
	install -d $(DESTDIR)$(libexecdir)/rcondor
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(docdir)

# install documentation
	install -m 644 ../author.txt $(DESTDIR)$(docdir)
	install -m 644 ../license.txt $(DESTDIR)$(docdir)
	install -m 644 ../README $(DESTDIR)$(docdir)

# install scripts
	install -m 755 rcondor.sh $(DESTDIR)$(libexecdir)/rcondor
	install -m 755 rcondor_mnt $(DESTDIR)$(bindir)
	install -m 755 rcondor_configure $(DESTDIR)$(bindir)

# create condor command links to rcondor.sh
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor.sh $(DESTDIR)$(bindir)/condor_q
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor.sh $(DESTDIR)$(bindir)/condor_rm
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor.sh $(DESTDIR)$(bindir)/condor_status
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor.sh $(DESTDIR)$(bindir)/condor_submit

.PHONY: uninstall
uninstall:
	rm -rf $(DESTDIR)$(libexecdir)/rcondor
	rm -rf $(DESTDIR)$(docdir)
	rm -f $(DESTDIR)$(bindir)/rcondor_mnt
	rm -f $(DESTDIR)$(bindir)/rcondor_configure
	rm -f $(DESTDIR)$(bindir)/condor_q
	rm -f $(DESTDIR)$(bindir)/condor_rm
	rm -f $(DESTDIR)$(bindir)/condor_status
	rm -f $(DESTDIR)$(bindir)/condor_submit
