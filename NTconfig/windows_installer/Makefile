CWD=$(CURDIR)
BSE=$(CWD)/../..
BASE=$(subst /,\\,$(BSE))
CONDORRELEASEDIR=$(BASE)\\release-dir

# for testing
#PATH=C:\Windows\system32;C:\condor\workspaces\v67\NTconfig;C:\Perl\bin


all: clean license config_file version_file condor_setup msi 
	cmd /c copy /y out\condor.mm\MSI\*.msi $(CONDORRELEASEDIR)

condor_setup: condor_setup.c 
	cl /DWIN32 /I $(BASE)\\src\\h /I $(BASE)\\src\\condor_includes condor_setup.c $(BASE)\\src\\condor_util_lib\\my_getopt.c

msi: condor_setup condor.mm
	cmd /c mm condor.mm

version_file:
	cmd /c copy /Y condor.ver.template condor.ver
	$(CONDORRELEASEDIR)/bin/condor_version.exe | awk '{print "; MsiName     = condor-" $$2 "-winnt50-x86"; exit}' >> condor.ver
	echo "; Guid.UpgradeCode = {EA1CEE00-C44F-4BC4-8035-440AA32C1F33}" >> condor.ver
	echo ";++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" >> condor.ver
	echo>> condor.ver
	echo>> condor.ver
	echo>> condor.ver
	echo ";############################################################################" >> condor.ver
	$(CONDORRELEASEDIR)/bin/condor_version.exe | awk '{print "VERSION : " $$2; exit}' >> condor.ver
	echo a | awk '{print "DATE    : "  strftime("%d %b %Y")}' >> condor.ver
	echo a | awk '{print "CHANGES : none"}' >> condor.ver
	echo>> condor.ver

config_file:
	awk -f $(BASE)\\src\\condor_examples\\convert_config_to_win32.awk $(BASE)\\src\\condor_examples\\condor_config.generic > $(CONDORRELEASEDIR)\\condor_config
	

license:
	perl txt2rtf.pl --font="Courier New" --points=7.5 $(BASE)\\src\\condor_release\\license.txt > license.rtf

clean:
	rm -rf out
	rm -rf *.exe *.obj
	rm -f condor.ver
	rm -f license.rtf
	rm -f $(CONDORRELEASEDIR)\\*.msi
