
##
## NMAKE file to build the param stubs. This file is NOT machine generated,
## so humans can feel free to edit it as necessary.
##

SRCDIR = $(MAKEDIR)\..\src\condor_c++_util

all : $(SRCDIR)\param_info_init.c

$(SRCDIR)\param_info_init.c : $(SRCDIR)\param_info.in $(SRCDIR)\param_info_c_generator.pl
	cd $(SRCDIR)
	perl ./param_info_c_generator.pl



