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

# install scripts in libexec
	install -m 755 rcondor.sh $(DESTDIR)$(libexecdir)/rcondor
	install -m 644 rcondor_config.source $(DESTDIR)$(libexecdir)/rcondor
	install -m 755 rcondor_mnt $(DESTDIR)$(libexecdir)/rcondor
	install -m 755 rcondor_unmnt $(DESTDIR)$(libexecdir)/rcondor
	install -m 755 rcondor_configure $(DESTDIR)$(bindir)

# create condor command links to rcondor.sh and others
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor_mnt $(DESTDIR)$(bindir)/rcondor_mnt
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor_unmnt $(DESTDIR)$(bindir)/rcondor_unmnt
	-ln -s $(DESTDIR)$(libexecdir)/rcondor/rcondor.sh $(DESTDIR)$(bindir)/rcondor_id
	./create_symlinks.sh $(DESTDIR)$(bindir) $(DESTDIR)$(libexecdir)/rcondor ./rcondor_symlinks.lst >/dev/null
	./create_setup_scripts.sh $(DESTDIR)$(prefix)

.PHONY: uninstall
uninstall:
	rm -rf $(DESTDIR)$(libexecdir)/rcondor
	rm -rf $(DESTDIR)$(docdir)
	rm -f $(DESTDIR)$(bindir)/rcondor_mnt
	rm -f $(DESTDIR)$(bindir)/rcondor_unmnt
	rm -f $(DESTDIR)$(bindir)/rcondor_id
	rm -f $(DESTDIR)$(bindir)/rcondor_configure
	./remove_symlinks.sh $(DESTDIR)$(bindir) ./rcondor_symlinks.lst >/dev/null
	rm -f $(DESTDIR)$(prefix)/rcondor_setup.sh $(DESTDIR)$(prefix)/rcondor_setup.csh
