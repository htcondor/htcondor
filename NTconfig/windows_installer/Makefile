CWD=$(CURDIR)
BSE=$(CWD)/../..
BASE=$(subst /,\\,$(BSE))
CONDORRELEASEDIR=$(BASE)\\release-dir
PATH=C:\Windows\system32;C:\condor\workspaces\v67\NTconfig;C:\Perl\bin


all: license config_file condor_setup msi 

condor_setup: condor_setup.c 
	cl /DWIN32 /I $(BASE)\\src\\h /I $(BASE)\\src\\condor_includes condor_setup.c $(BASE)\\src\\condor_util_lib\\my_getopt.c

msi: condor_setup condor.mm
	cmd /c mm condor.mm

config_file:
	awk -f $(BASE)\\src\\condor_examples\\convert_config_to_win32.awk $(BASE)\\src\\condor_examples\\condor_config.generic > $(CONDORRELEASEDIR)\\condor_config
	

license:
	perl txt2rf.pl --font="Courier New" --points=7.5 $(BASE)\\src\\condor_release\\license.txt > license.rtf

clean:
	rm -rf out
	rm -rf *.exe *.obj
